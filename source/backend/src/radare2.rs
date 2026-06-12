//! Optional radare2 backend for the disassembler tab (Qt parity).
//!
//! Shells out to the external `r2` binary in ONE call, asking it for several
//! pieces of analysis at once (sections, functions, strings, disassembly) and
//! reshapes the result into the same `DisassemblyResultDto` the embedded
//! iced-x86 backend produces — so the frontend parser stays unchanged.
//!
//! Why bother with r2 when iced-x86 covers x86 / x86-64? r2 handles ARM,
//! MIPS, RISC-V, and many quirky architectures iced-x86 doesn't touch, plus
//! it can apply name resolution from the binary's symbol table.

use std::collections::HashMap;
use std::path::{Path, PathBuf};

use crate::disassembly::{DisassemblyResultDto, DisassemblySyntaxOption};
use crate::win_command::command;

// Marker tokens we tell r2 to print between data sections in one script — far
// less fragile than two-pass r2 invocations.
const MARK_FN: &str = "___CREMNIY_FN___";
const MARK_STR: &str = "___CREMNIY_STR___";
const MARK_SEC: &str = "___CREMNIY_SEC___";
const MARK_DASM: &str = "___CREMNIY_DASM___";

#[derive(serde::Deserialize, Debug)]
struct R2Instruction {
    offset: u64,
    #[serde(default)]
    size: u32,
    #[serde(default)]
    bytes: Option<String>,
    #[serde(default)]
    opcode: Option<String>,
    #[serde(default)]
    disasm: Option<String>,
}

#[derive(serde::Deserialize, Debug, Default)]
struct R2Function {
    #[serde(default)]
    offset: u64,
    #[serde(default)]
    name: Option<String>,
}

#[derive(serde::Deserialize, Debug, Default)]
struct R2String {
    #[serde(default)]
    vaddr: Option<u64>,
    #[serde(default)]
    paddr: Option<u64>,
    #[serde(default)]
    string: Option<String>,
}

#[derive(serde::Deserialize, Debug, Default)]
struct R2Section {
    #[serde(default)]
    name: Option<String>,
    #[serde(default)]
    vaddr: Option<u64>,
    #[serde(default)]
    paddr: Option<u64>,
    #[serde(default)]
    size: Option<u64>,
    #[serde(default)]
    perm: Option<String>,
}

#[tauri::command]
pub fn disassemble_with_radare2(
    workspace_root: String,
    file_path: String,
    radare2_path: Option<String>,
    arch_hint: Option<String>,
    analysis_level: Option<String>,
    pre_commands: Option<String>,
    syntax: Option<DisassemblySyntaxOption>,
    instruction_limit: Option<usize>,
) -> Result<DisassemblyResultDto, String> {
    let root = canonical_workspace_directory(&workspace_root)?;
    let file = canonical_workspace_file(&root, &file_path)?;

    let r2 = resolve_r2_path(radare2_path.as_deref())?;
    let analysis = match analysis_level.as_deref().unwrap_or("none") {
        "aaa" => "aaa",
        "aa" => "aa",
        _ => "",
    };
    let syntax_cmd = match syntax.unwrap_or(DisassemblySyntaxOption::Intel) {
        DisassemblySyntaxOption::Intel => "e asm.syntax=intel",
        DisassemblySyntaxOption::Att => "e asm.syntax=att",
    };
    let limit = instruction_limit.filter(|v| *v > 0).unwrap_or(2_000);
    let arch = arch_hint
        .as_deref()
        .map(str::trim)
        .filter(|s| !s.is_empty())
        .map(str::to_string);

    // Build the script once — r2 only spins up its analysis pipeline a single
    // time this way (vs running it 4 times for separate aflj/izj/iSj/pdj
    // invocations).
    let mut script = String::new();
    let pre = pre_commands.unwrap_or_default();
    for line in pre.lines() {
        let trimmed = line.trim();
        if !trimmed.is_empty() {
            script.push_str(trimmed);
            script.push(';');
        }
    }
    if let Some(ref a) = arch {
        // User-asserted ISA — overrides r2's auto-detect (useful for raw blobs
        // or when r2 misidentifies the format).
        script.push_str(&format!("e asm.arch={a};"));
    }
    script.push_str(syntax_cmd);
    script.push(';');
    if !analysis.is_empty() {
        script.push_str(analysis);
        script.push(';');
    }
    // ?e prints a literal line on stdout — used as a section separator so we
    // can split the output cleanly even if any single JSON document contains
    // brackets that confuse a naive parser.
    script.push_str(&format!("?e {MARK_FN};aflj;"));
    script.push_str(&format!("?e {MARK_STR};izj;"));
    script.push_str(&format!("?e {MARK_SEC};iSj;"));
    script.push_str(&format!("?e {MARK_DASM};pdj {limit} @ entry0"));

    let output = command(&r2)
        .args(["-q", "-c", &script, "--"])
        .arg(&file)
        .env("LANG", "C")
        .env("LC_ALL", "C")
        .output()
        .map_err(|e| format!("spawn radare2: {e}"))?;

    let stderr = String::from_utf8_lossy(&output.stderr).into_owned();
    if !output.status.success() {
        return Err(format!(
            "radare2 exited with status {:?}: {}",
            output.status.code(),
            stderr.trim()
        ));
    }

    let raw_stdout = String::from_utf8_lossy(&output.stdout);
    let parts = split_marked_blocks(&raw_stdout);

    let functions: Vec<R2Function> = parts
        .get(MARK_FN)
        .map(|s| parse_json_or_empty(s))
        .unwrap_or_default();
    let strings: Vec<R2String> = parts
        .get(MARK_STR)
        .map(|s| parse_json_or_empty(s))
        .unwrap_or_default();
    let sections: Vec<R2Section> = parts
        .get(MARK_SEC)
        .map(|s| parse_json_or_empty(s))
        .unwrap_or_default();
    let instructions: Vec<R2Instruction> = parts
        .get(MARK_DASM)
        .map(|s| parse_json_or_empty(s))
        .unwrap_or_default();

    if instructions.is_empty() {
        // Pretty error rather than emitting an empty objdump-shaped result —
        // most often this means the user picked the wrong arch / r2 couldn't
        // find a sensible entry point.
        return Err(format!(
            "radare2 produced no instructions. Check arch / pre-commands / entry point.\nstderr: {}",
            stderr.trim()
        ));
    }

    let function_names: HashMap<u64, String> = functions
        .into_iter()
        .filter_map(|f| f.name.map(|n| (f.offset, n)))
        .collect();
    let strings_by_addr: HashMap<u64, String> = strings
        .into_iter()
        .filter_map(|s| {
            let addr = s.vaddr.or(s.paddr)?;
            let text = s.string?;
            Some((addr, text))
        })
        .collect();

    // Re-emit in objdump-style so the existing frontend parser handles it.
    let mut stdout = String::new();
    stdout.push_str("\nDisassembly of section .text:\n\n");
    if let Some(first) = instructions.first() {
        let name = function_names
            .get(&first.offset)
            .cloned()
            .unwrap_or_else(|| "radare2".to_string());
        stdout.push_str(&format!("{:016x} <{name}>:\n", first.offset));
    }
    for insn in &instructions {
        // If a fresh function starts here, emit an objdump-style label line.
        if let Some(name) = function_names.get(&insn.offset) {
            stdout.push_str(&format!("\n{:016x} <{name}>:\n", insn.offset));
        }

        let bytes = insn.bytes.clone().unwrap_or_default();
        let bytes_spaced = bytes
            .as_bytes()
            .chunks(2)
            .map(|c| std::str::from_utf8(c).unwrap_or("??"))
            .collect::<Vec<_>>()
            .join(" ");
        let text = insn
            .disasm
            .clone()
            .or_else(|| insn.opcode.clone())
            .unwrap_or_else(|| "?".to_string());

        // Best-effort string-reference comment — scan for any hex literal in
        // the instruction text and look it up in the strings table.
        let comment = comment_for_instruction(&text, &strings_by_addr);

        if comment.is_empty() {
            stdout.push_str(&format!(
                "  {:>8x}:\t{:<22}\t{}\n",
                insn.offset, bytes_spaced, text,
            ));
        } else {
            stdout.push_str(&format!(
                "  {:>8x}:\t{:<22}\t{:<32}\t# {}\n",
                insn.offset, bytes_spaced, text, comment,
            ));
        }
    }

    // Section headers — emit real ones if r2 gave us any executable sections,
    // otherwise a single synthetic .text line so the frontend section filter
    // has something to display.
    let mut headers = String::new();
    headers.push_str(
        "Idx Name          Size      VMA               LMA               File off  Algn\n",
    );
    let exec_sections: Vec<&R2Section> = sections
        .iter()
        .filter(|s| s.perm.as_deref().map(|p| p.contains('x')).unwrap_or(false))
        .collect();
    if exec_sections.is_empty() {
        if let Some(first) = instructions.first() {
            let total: u64 = instructions.iter().map(|i| i.size as u64).sum();
            headers.push_str(&format!(
                "  0 {:<13} {:08x}  {:016x}  {:016x}  {:08x}  2**0\n",
                ".text", total, first.offset, first.offset, 0,
            ));
        }
    } else {
        for (idx, sec) in exec_sections.iter().enumerate() {
            let name = sec.name.clone().unwrap_or_else(|| format!("sec{idx}"));
            headers.push_str(&format!(
                "{:>3} {:<13} {:08x}  {:016x}  {:016x}  {:08x}  2**0\n",
                idx,
                name,
                sec.size.unwrap_or(0),
                sec.vaddr.unwrap_or(0),
                sec.vaddr.unwrap_or(0),
                sec.paddr.unwrap_or(0),
            ));
        }
    }

    Ok(DisassemblyResultDto {
        executable: format!("radare2 ({})", r2.to_string_lossy()),
        args: vec![
            format!("analysis={}", if analysis.is_empty() { "none" } else { analysis }),
            format!("syntax={}", syntax_label(syntax)),
            format!("arch={}", arch.unwrap_or_else(|| "auto".to_string())),
            format!("limit={limit}"),
            format!("functions={}", function_names.len()),
            format!("strings={}", strings_by_addr.len()),
        ],
        cwd: root.to_string_lossy().into_owned(),
        file_path: file.to_string_lossy().into_owned(),
        stdout,
        stderr,
        status_code: Some(0),
        section_headers_stdout: headers,
        section_headers_stderr: String::new(),
        section_headers_status_code: Some(0),
    })
}

fn syntax_label(syntax: Option<DisassemblySyntaxOption>) -> &'static str {
    match syntax.unwrap_or(DisassemblySyntaxOption::Intel) {
        DisassemblySyntaxOption::Intel => "intel",
        DisassemblySyntaxOption::Att => "att",
    }
}

/// Split the r2 stdout into named blocks using the marker lines we asked it
/// to print. Blocks come back in script order; we collect by marker for O(1)
/// lookup.
fn split_marked_blocks(raw: &str) -> HashMap<&'static str, String> {
    let markers = [MARK_FN, MARK_STR, MARK_SEC, MARK_DASM];
    let mut out: HashMap<&'static str, String> = HashMap::new();
    let mut current: Option<&'static str> = None;
    let mut acc = String::new();

    for line in raw.lines() {
        let trimmed = line.trim();
        if let Some(&m) = markers.iter().find(|m| trimmed == **m) {
            if let Some(name) = current.take() {
                out.insert(name, std::mem::take(&mut acc));
            }
            current = Some(m);
            continue;
        }
        if current.is_some() {
            acc.push_str(line);
            acc.push('\n');
        }
    }
    if let Some(name) = current.take() {
        out.insert(name, acc);
    }
    out
}

fn parse_json_or_empty<T>(raw: &str) -> Vec<T>
where
    T: serde::de::DeserializeOwned,
{
    let Some(start) = raw.find('[') else {
        return Vec::new();
    };
    let Some(end_rel) = raw[start..].rfind(']') else {
        return Vec::new();
    };
    serde_json::from_str(&raw[start..=start + end_rel]).unwrap_or_default()
}

/// Scan an instruction's textual operands for any hex literal that matches a
/// known string address and return a comment ready to append. Conservative:
/// only matches `0x[0-9a-fA-F]+` tokens to avoid false positives on plain
/// decimal offsets.
fn comment_for_instruction(text: &str, strings_by_addr: &HashMap<u64, String>) -> String {
    if strings_by_addr.is_empty() {
        return String::new();
    }
    let mut i = 0;
    let bytes = text.as_bytes();
    while i + 2 < bytes.len() {
        if bytes[i] == b'0' && (bytes[i + 1] == b'x' || bytes[i + 1] == b'X') {
            let mut j = i + 2;
            while j < bytes.len() && bytes[j].is_ascii_hexdigit() {
                j += 1;
            }
            if j > i + 2 {
                if let Ok(addr) = u64::from_str_radix(&text[i + 2..j], 16) {
                    if let Some(s) = strings_by_addr.get(&addr) {
                        return format!("\"{}\"", s.replace('"', "\\\""));
                    }
                }
                i = j;
                continue;
            }
        }
        i += 1;
    }
    String::new()
}

fn resolve_r2_path(configured: Option<&str>) -> Result<PathBuf, String> {
    let trimmed = configured.map(str::trim).unwrap_or("");
    if !trimmed.is_empty() {
        let p = PathBuf::from(trimmed);
        if !p.is_absolute() {
            return Err(String::from("radare2 path must be absolute"));
        }
        if !p.exists() {
            return Err(format!("radare2 path does not exist: {}", p.display()));
        }
        return Ok(p);
    }
    for name in ["r2", "radare2"] {
        if let Some(resolved) = which_on_path(name) {
            return Ok(resolved);
        }
    }
    Err(String::from(
        "radare2 not found on PATH. Set the path explicitly in Settings → Disassembler.",
    ))
}

fn which_on_path(name: &str) -> Option<PathBuf> {
    let path_env = std::env::var_os("PATH")?;
    let path_ext = std::env::var("PATHEXT").unwrap_or_default();
    let exts: Vec<String> = if path_ext.is_empty() {
        vec![String::new()]
    } else {
        std::iter::once(String::new())
            .chain(path_ext.split(';').map(|e| e.to_string()))
            .collect()
    };
    for dir in std::env::split_paths(&path_env) {
        for ext in &exts {
            let candidate = if ext.is_empty() {
                dir.join(name)
            } else {
                dir.join(format!("{name}{ext}"))
            };
            if candidate.is_file() {
                return Some(candidate);
            }
        }
    }
    None
}

fn canonical_workspace_directory(workspace_root: &str) -> Result<PathBuf, String> {
    let root = PathBuf::from(workspace_root.trim());
    if root.as_os_str().is_empty() {
        return Err(String::from("workspace_root must not be empty"));
    }
    root.canonicalize()
        .map_err(|e| format!("workspace_root: {e}"))
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
    Ok(file_canon)
}
