//! Cross-format binary inspection helpers — symbol table, imports, exports.
//!
//! Frontend tabs (Symbol Table, Imports/Exports, Function List) call into
//! these commands instead of re-parsing the disassembled text. ELF, PE, and
//! Mach-O are all routed through `goblin`; raw files return an empty list.

use std::path::{Path, PathBuf};

use goblin::Object;

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SymbolDto {
    name: String,
    address: String,
    size: Option<u64>,
    kind: String,
    binding: String,
    source: String,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SectionDto {
    name: String,
    vma: String,
    size: u64,
    file_offset: u64,
    is_executable: bool,
    is_writable: bool,
    is_readable: bool,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct BinaryAnalysisDto {
    format: String,
    bitness: u32,
    sections: Vec<SectionDto>,
    symbols: Vec<SymbolDto>,
    /// True count before capping, so the UI can show "showing N of M". A debug
    /// build can hold millions of symbols; the returned `symbols` is bounded to
    /// `MAX_LIST` but this is the real total.
    sections_total: usize,
    symbols_total: usize,
}

/// Hard cap on each returned list. A large binary (e.g. a debug exe) can carry
/// millions of symbols; serialising them all across the IPC bridge OOMs the
/// webview. Bound the DTO here and let the UI window the bounded slice — the
/// `*_total` fields carry the true counts. Mirrors the Strings tool's count cap.
const MAX_LIST: usize = 5000;

/// Truncate `list` to `MAX_LIST` and return its original length. Keeps the
/// first N entries, matching the Strings tool (first N strings of the file).
fn cap_list<T>(list: &mut Vec<T>) -> usize {
    let total = list.len();
    if total > MAX_LIST {
        list.truncate(MAX_LIST);
    }
    total
}

#[tauri::command]
pub fn analyze_binary(workspace_root: String, file_path: String) -> Result<BinaryAnalysisDto, String> {
    let root = canonical_workspace_directory(&workspace_root)?;
    let file = canonical_workspace_file(&root, &file_path)?;
    let bytes = std::fs::read(&file).map_err(|e| format!("read file: {e}"))?;

    if bytes.is_empty() {
        return Err(String::from("file is empty"));
    }

    let parsed = Object::parse(&bytes).map_err(|e| format!("parse binary: {e}"))?;
    match parsed {
        Object::Elf(elf) => analyze_elf(&elf),
        Object::PE(pe) => analyze_pe(&pe),
        Object::Mach(_) => Ok(BinaryAnalysisDto {
            format: "Mach-O".to_string(),
            bitness: 64,
            sections: vec![],
            symbols: vec![],
            sections_total: 0,
            symbols_total: 0,
        }),
        _ => Ok(BinaryAnalysisDto {
            format: "Raw".to_string(),
            bitness: 0,
            sections: vec![],
            symbols: vec![],
            sections_total: 0,
            symbols_total: 0,
        }),
    }
}

fn analyze_elf(elf: &goblin::elf::Elf) -> Result<BinaryAnalysisDto, String> {
    use goblin::elf::section_header::{SHF_ALLOC, SHF_EXECINSTR, SHF_WRITE};

    let bitness = if elf.is_64 { 64 } else { 32 };

    let mut sections = Vec::new();
    for sh in &elf.section_headers {
        let name = elf
            .shdr_strtab
            .get_at(sh.sh_name)
            .unwrap_or("?")
            .to_string();
        if name.is_empty() {
            continue;
        }
        sections.push(SectionDto {
            name,
            vma: format!("{:#018x}", sh.sh_addr),
            size: sh.sh_size,
            file_offset: sh.sh_offset,
            is_executable: (sh.sh_flags & u64::from(SHF_EXECINSTR)) != 0,
            is_writable: (sh.sh_flags & u64::from(SHF_WRITE)) != 0,
            is_readable: (sh.sh_flags & u64::from(SHF_ALLOC)) != 0,
        });
    }

    let mut symbols = Vec::new();
    let dynstrtab = &elf.dynstrtab;
    for sym in elf.dynsyms.iter() {
        let name = dynstrtab.get_at(sym.st_name).unwrap_or("?").to_string();
        symbols.push(SymbolDto {
            name,
            address: format!("{:#018x}", sym.st_value),
            size: Some(sym.st_size),
            kind: elf_symbol_kind(sym.st_type()),
            binding: elf_symbol_binding(sym.st_bind()),
            source: if sym.is_import() {
                ".dynsym (import)".to_string()
            } else {
                ".dynsym".to_string()
            },
        });
    }
    let strtab = &elf.strtab;
    for sym in elf.syms.iter() {
        let name = strtab.get_at(sym.st_name).unwrap_or("?").to_string();
        if name.is_empty() {
            continue;
        }
        symbols.push(SymbolDto {
            name,
            address: format!("{:#018x}", sym.st_value),
            size: Some(sym.st_size),
            kind: elf_symbol_kind(sym.st_type()),
            binding: elf_symbol_binding(sym.st_bind()),
            source: ".symtab".to_string(),
        });
    }

    let sections_total = cap_list(&mut sections);
    let symbols_total = cap_list(&mut symbols);
    Ok(BinaryAnalysisDto {
        format: "ELF".to_string(),
        bitness,
        sections,
        symbols,
        sections_total,
        symbols_total,
    })
}

fn elf_symbol_kind(st_type: u8) -> String {
    match st_type {
        0 => "notype",
        1 => "object",
        2 => "func",
        3 => "section",
        4 => "file",
        _ => "other",
    }
    .to_string()
}

fn elf_symbol_binding(st_bind: u8) -> String {
    match st_bind {
        0 => "local",
        1 => "global",
        2 => "weak",
        _ => "other",
    }
    .to_string()
}

fn analyze_pe(pe: &goblin::pe::PE) -> Result<BinaryAnalysisDto, String> {
    const IMAGE_SCN_MEM_EXECUTE: u32 = 0x2000_0000;
    const IMAGE_SCN_MEM_WRITE: u32 = 0x8000_0000;
    const IMAGE_SCN_MEM_READ: u32 = 0x4000_0000;

    let bitness = if pe.is_64 { 64 } else { 32 };
    let image_base = pe.image_base as u64;

    let mut sections = Vec::new();
    for sh in &pe.sections {
        let name = sh.name().unwrap_or("?").to_string();
        sections.push(SectionDto {
            name,
            vma: format!("{:#018x}", image_base.saturating_add(sh.virtual_address as u64)),
            size: sh.size_of_raw_data as u64,
            file_offset: sh.pointer_to_raw_data as u64,
            is_executable: (sh.characteristics & IMAGE_SCN_MEM_EXECUTE) != 0,
            is_writable: (sh.characteristics & IMAGE_SCN_MEM_WRITE) != 0,
            is_readable: (sh.characteristics & IMAGE_SCN_MEM_READ) != 0,
        });
    }

    let mut symbols = Vec::new();
    for import in &pe.imports {
        symbols.push(SymbolDto {
            name: import.name.to_string(),
            address: format!("{:#018x}", image_base.saturating_add(import.rva as u64)),
            size: Some(import.size as u64),
            kind: "func".to_string(),
            binding: "global".to_string(),
            source: format!("import: {}", import.dll),
        });
    }
    for export in &pe.exports {
        symbols.push(SymbolDto {
            name: export
                .name
                .map(|s| s.to_string())
                .unwrap_or_else(|| "<unnamed>".to_string()),
            address: format!("{:#018x}", image_base.saturating_add(export.rva as u64)),
            size: Some(export.size as u64),
            kind: "func".to_string(),
            binding: "global".to_string(),
            source: "export".to_string(),
        });
    }

    let sections_total = cap_list(&mut sections);
    let symbols_total = cap_list(&mut symbols);
    Ok(BinaryAnalysisDto {
        format: "PE".to_string(),
        bitness,
        sections,
        symbols,
        sections_total,
        symbols_total,
    })
}

fn canonical_workspace_directory(workspace_root: &str) -> Result<PathBuf, String> {
    let root = PathBuf::from(workspace_root.trim());
    if root.as_os_str().is_empty() {
        return Err(String::from("workspace_root must not be empty"));
    }
    let root_canon = root
        .canonicalize()
        .map_err(|e| format!("workspace_root: {e}"))?;
    let meta = std::fs::metadata(&root_canon).map_err(|e| e.to_string())?;
    if !meta.is_dir() {
        return Err(String::from("workspace_root is not a directory"));
    }
    Ok(root_canon)
}

fn canonical_workspace_file(root_canon: &Path, file_path: &str) -> Result<PathBuf, String> {
    let file = PathBuf::from(file_path.trim());
    if file.as_os_str().is_empty() {
        return Err(String::from("file_path must not be empty"));
    }
    let file_canon = file.canonicalize().map_err(|e| format!("file_path: {e}"))?;
    if !file_canon.starts_with(root_canon) {
        return Err(String::from("file_path is outside workspace"));
    }
    let meta = std::fs::metadata(&file_canon).map_err(|e| e.to_string())?;
    if !meta.is_file() {
        return Err(String::from("file_path is not a regular file"));
    }
    Ok(file_canon)
}
