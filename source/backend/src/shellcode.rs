//! Shellcode assembler — shells out to `nasm` (or its alias) to compile a
//! short assembly snippet and returns the resulting machine bytes.
//!
//! Why NASM and not Keystone-rs? Keystone is a beast of a C dependency; nasm
//! is widely installed and zero-cost to invoke. If a user doesn't have nasm
//! we return a clear error pointing them at `apt install nasm` / `choco
//! install nasm`. No silent fallback — the user explicitly chose this tool.

use std::io::Write;
use std::path::PathBuf;

use crate::win_command::command;

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ShellcodeResult {
    pub bytes: Vec<u8>,
    pub stderr: String,
    pub nasm_path: String,
}

#[tauri::command]
pub fn assemble_with_nasm(
    source: String,
    bits: u32,
    nasm_path: Option<String>,
) -> Result<ShellcodeResult, String> {
    if source.trim().is_empty() {
        return Err(String::from("Source is empty"));
    }
    let bits_arg = match bits {
        16 | 32 | 64 => format!("-f bin"),
        _ => return Err(format!("Unsupported bitness: {bits}. Use 16, 32, or 64.")),
    };
    let _ = bits_arg; // placeholder — we set -f bin always; bits goes via BITS prefix.

    let nasm = resolve_nasm(nasm_path.as_deref())?;

    let mut src_file = tempfile::Builder::new()
        .prefix("cremniy-asm-")
        .suffix(".asm")
        .tempfile()
        .map_err(|e| format!("create temp source: {e}"))?;

    // Prepend a BITS directive so the user doesn't have to write it.
    writeln!(src_file, "BITS {bits}").map_err(|e| e.to_string())?;
    src_file
        .write_all(source.as_bytes())
        .map_err(|e| e.to_string())?;
    src_file.flush().map_err(|e| e.to_string())?;

    let out_path = src_file.path().with_extension("bin");

    let output = command(&nasm)
        .args(["-f", "bin", "-o"])
        .arg(&out_path)
        .arg(src_file.path())
        .output()
        .map_err(|e| format!("spawn nasm: {e}"))?;

    let stderr = String::from_utf8_lossy(&output.stderr).into_owned();
    if !output.status.success() {
        return Err(format!(
            "nasm exited with status {:?}:\n{}",
            output.status.code(),
            stderr.trim()
        ));
    }

    let bytes = std::fs::read(&out_path).map_err(|e| format!("read assembled bytes: {e}"))?;
    // Best-effort cleanup — ignore errors, OS will tidy up on reboot anyway.
    let _ = std::fs::remove_file(&out_path);

    Ok(ShellcodeResult {
        bytes,
        stderr,
        nasm_path: nasm.to_string_lossy().into_owned(),
    })
}

fn resolve_nasm(configured: Option<&str>) -> Result<PathBuf, String> {
    let trimmed = configured.map(str::trim).unwrap_or("");
    if !trimmed.is_empty() {
        let p = PathBuf::from(trimmed);
        if !p.exists() {
            return Err(format!("nasm path does not exist: {}", p.display()));
        }
        return Ok(p);
    }
    if let Some(found) = which("nasm") {
        return Ok(found);
    }
    Err(String::from(
        "nasm not found on PATH. Install it (apt install nasm / choco install nasm / brew install nasm) or set the path in Settings.",
    ))
}

fn which(name: &str) -> Option<PathBuf> {
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
