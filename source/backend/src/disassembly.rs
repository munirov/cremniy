//! Embedded x86 / x86-64 disassembler.
//!
//! No external objdump. Binary parsing via `goblin` (ELF / PE / Mach-O / raw),
//! decoding via `iced-x86` (pure-Rust Intel/AT&T formatter). Output is emitted
//! in an objdump-compatible textual layout so the existing frontend parser in
//! `domain/disassembly/disassembly.ts` keeps working without changes.

use std::path::{Path, PathBuf};

use goblin::Object;
use iced_x86::{Decoder, DecoderOptions, Formatter, GasFormatter, Instruction, IntelFormatter};

const X86_64_DEFAULT_BITNESS: u32 = 64;

#[derive(Clone, Copy, Debug, Eq, PartialEq, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
pub enum DisassemblySyntaxOption {
    Intel,
    Att,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct DisassemblyResultDto {
    // `pub(crate)` so the alternate radare2 backend in `radare2.rs` can
    // construct one without us inventing a 10-argument `new()` constructor.
    pub(crate) executable: String,
    pub(crate) args: Vec<String>,
    pub(crate) cwd: String,
    pub(crate) file_path: String,
    pub(crate) stdout: String,
    pub(crate) stderr: String,
    pub(crate) status_code: Option<i32>,
    pub(crate) section_headers_stdout: String,
    pub(crate) section_headers_stderr: String,
    pub(crate) section_headers_status_code: Option<i32>,
}

#[tauri::command]
pub fn disassemble_workspace_file(
    workspace_root: String,
    file_path: String,
    // The following two args are kept on the signature for frontend
    // compatibility but are no longer used — we don't shell out and the
    // architecture is inferred from the binary.
    objdump_path: Option<String>,
    arch_hint: Option<String>,
    syntax: Option<DisassemblySyntaxOption>,
    instruction_limit: Option<usize>,
) -> Result<DisassemblyResultDto, String> {
    let _ = objdump_path;
    let _ = arch_hint;

    let root = canonical_workspace_directory(&workspace_root)?;
    let file = canonical_workspace_file(&root, &file_path)?;
    let bytes = std::fs::read(&file).map_err(|e| format!("read file: {e}"))?;

    let syntax = syntax.unwrap_or(DisassemblySyntaxOption::Intel);
    let sections = extract_executable_sections(&bytes)?;

    let limit = instruction_limit
        .filter(|value| *value > 0)
        .unwrap_or(usize::MAX);

    let mut stdout = String::new();
    let mut header_stdout = String::new();
    header_stdout.push_str(
        "Idx Name          Size      VMA               LMA               File off  Algn\n",
    );

    let mut total_emitted = 0usize;
    let mut truncated = false;

    for (idx, sec) in sections.iter().enumerate() {
        header_stdout.push_str(&format!(
            "{:>3} {:<13} {:08x}  {:016x}  {:016x}  {:08x}  2**0\n",
            idx, sec.name, sec.size, sec.vaddr, sec.vaddr, sec.file_offset,
        ));
    }

    'sections: for sec in &sections {
        stdout.push_str(&format!("\nDisassembly of section {}:\n\n", sec.name));
        stdout.push_str(&format!(
            "{:016x} <{}>:\n",
            sec.vaddr,
            strip_leading_dot(&sec.name),
        ));

        let mut decoder = Decoder::with_ip(sec.bitness, &sec.bytes, sec.vaddr, DecoderOptions::NONE);
        let mut formatter: Box<dyn Formatter> = match syntax {
            DisassemblySyntaxOption::Att => Box::new(GasFormatter::new()),
            DisassemblySyntaxOption::Intel => Box::new(IntelFormatter::new()),
        };

        let mut instruction = Instruction::default();
        while decoder.can_decode() {
            let pre = decoder.position();
            decoder.decode_out(&mut instruction);
            let post = decoder.position();

            let raw = &sec.bytes[pre..post];
            let bytes_str = raw
                .iter()
                .map(|b| format!("{:02x}", b))
                .collect::<Vec<_>>()
                .join(" ");

            let mut text = String::new();
            formatter.format(&instruction, &mut text);

            stdout.push_str(&format!(
                "  {:>8x}:\t{:<22}\t{}\n",
                instruction.ip(),
                bytes_str,
                text,
            ));

            total_emitted += 1;
            if total_emitted >= limit {
                truncated = true;
                break 'sections;
            }
        }
    }

    let stderr = if truncated {
        format!(
            "Cremniy returned at most {limit} disassembled instruction row(s).\n",
            limit = limit,
        )
    } else {
        String::new()
    };

    Ok(DisassemblyResultDto {
        executable: String::from("iced-x86 (embedded)"),
        args: vec![
            format!("bitness={}", first_bitness(&sections)),
            format!("syntax={}", syntax_label(syntax)),
        ],
        cwd: root.to_string_lossy().into_owned(),
        file_path: file.to_string_lossy().into_owned(),
        stdout,
        stderr,
        status_code: Some(0),
        section_headers_stdout: header_stdout,
        section_headers_stderr: String::new(),
        section_headers_status_code: Some(0),
    })
}

#[tauri::command]
pub fn test_objdump_tool(
    workspace_root: Option<String>,
    objdump_path: Option<String>,
) -> Result<String, String> {
    let _ = workspace_root;
    let _ = objdump_path;
    Ok(String::from(
        "Cremniy embedded disassembler is available (iced-x86 + goblin). No external objdump required.",
    ))
}

#[derive(Debug)]
struct ExecutableSection {
    name: String,
    bitness: u32,
    bytes: Vec<u8>,
    vaddr: u64,
    file_offset: u64,
    size: u64,
}

fn extract_executable_sections(bytes: &[u8]) -> Result<Vec<ExecutableSection>, String> {
    if bytes.is_empty() {
        return Err(String::from("file is empty"));
    }

    let parsed = Object::parse(bytes);
    match parsed {
        Ok(Object::Elf(elf)) => extract_elf_sections(&elf, bytes),
        Ok(Object::PE(pe)) => extract_pe_sections(&pe, bytes),
        Ok(Object::Mach(_)) => {
            Err(String::from("Mach-O binaries are not yet supported."))
        }
        // Unknown / unrecognized container — fall back to treating the file as
        // raw x86-64 code so .bin firmware and detached blobs still work.
        Ok(_) | Err(_) => Ok(vec![ExecutableSection {
            name: String::from(".text"),
            bitness: X86_64_DEFAULT_BITNESS,
            bytes: bytes.to_vec(),
            vaddr: 0,
            file_offset: 0,
            size: bytes.len() as u64,
        }]),
    }
}

fn extract_elf_sections(
    elf: &goblin::elf::Elf,
    bytes: &[u8],
) -> Result<Vec<ExecutableSection>, String> {
    use goblin::elf::header::{EM_386, EM_X86_64};
    use goblin::elf::section_header::SHF_EXECINSTR;

    let machine = elf.header.e_machine;
    if machine != EM_386 && machine != EM_X86_64 {
        return Err(format!(
            "Unsupported ELF architecture (e_machine={machine}). Only x86 and x86-64 are supported."
        ));
    }

    let bitness = if elf.is_64 { 64 } else { 32 };

    let mut sections = Vec::new();
    for sh in &elf.section_headers {
        if (sh.sh_flags & u64::from(SHF_EXECINSTR)) == 0 {
            continue;
        }
        if sh.sh_size == 0 {
            continue;
        }
        let offset = sh.sh_offset as usize;
        let size = sh.sh_size as usize;
        if offset == 0 || offset.saturating_add(size) > bytes.len() {
            continue;
        }
        let name = elf
            .shdr_strtab
            .get_at(sh.sh_name)
            .unwrap_or("?")
            .to_string();
        sections.push(ExecutableSection {
            name,
            bitness,
            bytes: bytes[offset..offset + size].to_vec(),
            vaddr: sh.sh_addr,
            file_offset: sh.sh_offset,
            size: sh.sh_size,
        });
    }

    if sections.is_empty() {
        return Err(String::from(
            "No executable sections found in this ELF binary.",
        ));
    }

    Ok(sections)
}

fn extract_pe_sections(
    pe: &goblin::pe::PE,
    bytes: &[u8],
) -> Result<Vec<ExecutableSection>, String> {
    // PE machine codes from winnt.h.
    const IMAGE_FILE_MACHINE_I386: u16 = 0x14c;
    const IMAGE_FILE_MACHINE_AMD64: u16 = 0x8664;
    const IMAGE_SCN_MEM_EXECUTE: u32 = 0x2000_0000;

    let machine = pe.header.coff_header.machine;
    if machine != IMAGE_FILE_MACHINE_I386 && machine != IMAGE_FILE_MACHINE_AMD64 {
        return Err(format!(
            "Unsupported PE machine ({machine:#x}). Only x86 and x86-64 are supported."
        ));
    }

    let bitness = if pe.is_64 { 64 } else { 32 };
    let image_base = pe.image_base as u64;

    let mut sections = Vec::new();
    for sh in &pe.sections {
        if (sh.characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 {
            continue;
        }
        let offset = sh.pointer_to_raw_data as usize;
        let size = sh.size_of_raw_data as usize;
        if size == 0 || offset == 0 || offset.saturating_add(size) > bytes.len() {
            continue;
        }
        let name = sh.name().unwrap_or("?").to_string();
        let vaddr = image_base.saturating_add(sh.virtual_address as u64);
        sections.push(ExecutableSection {
            name,
            bitness,
            bytes: bytes[offset..offset + size].to_vec(),
            vaddr,
            file_offset: sh.pointer_to_raw_data as u64,
            size: sh.size_of_raw_data as u64,
        });
    }

    if sections.is_empty() {
        return Err(String::from(
            "No executable sections found in this PE binary.",
        ));
    }

    Ok(sections)
}

fn first_bitness(sections: &[ExecutableSection]) -> u32 {
    sections.first().map(|s| s.bitness).unwrap_or(X86_64_DEFAULT_BITNESS)
}

fn syntax_label(syntax: DisassemblySyntaxOption) -> &'static str {
    match syntax {
        DisassemblySyntaxOption::Intel => "intel",
        DisassemblySyntaxOption::Att => "att",
    }
}

fn strip_leading_dot(name: &str) -> &str {
    name.strip_prefix('.').unwrap_or(name)
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
