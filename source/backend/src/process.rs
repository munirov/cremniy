//! One-shot workspace command runner (build/run a program in the workspace).
//!
//! Hardening: cwd must resolve inside the workspace root; explicit argv (no
//! shell); wall-clock timeout via watchdog; per-stream output cap.
//! Docs: documentation/architecture/AGENT_CONTROL.md (Safety section).

use std::io::Read;
use std::path::{Path, PathBuf};
use std::process::Stdio;

use crate::win_command::command;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

const DEFAULT_TIMEOUT_MS: u64 = 120_000;
const MAX_TIMEOUT_MS: u64 = 600_000;
/// Per-stream capture cap (stdout and stderr each).
const MAX_STREAM_BYTES: usize = 2 * 1024 * 1024;
const POLL_INTERVAL: Duration = Duration::from_millis(50);

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ProcessResultDto {
    program: String,
    args: Vec<String>,
    cwd: String,
    stdout: String,
    stderr: String,
    status_code: Option<i32>,
    timed_out: bool,
    duration_ms: u64,
}

#[tauri::command]
pub fn run_workspace_command(
    workspace_root: String,
    program: String,
    args: Option<Vec<String>>,
    relative_cwd: Option<String>,
    timeout_ms: Option<u64>,
) -> Result<ProcessResultDto, String> {
    let root_canon = canonical_workspace_directory(&workspace_root)?;
    let cwd = resolve_cwd(&root_canon, relative_cwd.as_deref())?;
    let program = program.trim().to_string();
    if program.is_empty() {
        return Err(String::from("program must not be empty"));
    }
    let args = args.unwrap_or_default();
    let timeout = Duration::from_millis(
        timeout_ms
            .unwrap_or(DEFAULT_TIMEOUT_MS)
            .clamp(1, MAX_TIMEOUT_MS),
    );

    // Resolve a freshly-built binary that lives in the workspace cwd. Without
    // this, `process.run { program: "hello" }` after a build fails on every OS
    // because Command searches PATH, not the cwd. We only special-case bare
    // names (no separators) that actually exist in cwd; anything else (PATH
    // tools like `cargo`, or explicit `./x`) is left untouched.
    let resolved_program = resolve_program_in_cwd(&cwd, &program);

    let started = Instant::now();
    let mut child = command(&resolved_program)
        .args(&args)
        .current_dir(&cwd)
        .stdin(Stdio::null())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("failed to start '{program}': {e}"))?;

    let stdout_handle = child.stdout.take().map(spawn_capture);
    let stderr_handle = child.stderr.take().map(spawn_capture);

    let pid = child.id();
    let finished = Arc::new(AtomicBool::new(false));
    let timed_out = Arc::new(AtomicBool::new(false));

    let watchdog = {
        let finished = Arc::clone(&finished);
        let timed_out = Arc::clone(&timed_out);
        thread::spawn(move || {
            let deadline = Instant::now() + timeout;
            while Instant::now() < deadline {
                if finished.load(Ordering::SeqCst) {
                    return;
                }
                thread::sleep(POLL_INTERVAL);
            }
            if !finished.load(Ordering::SeqCst) {
                timed_out.store(true, Ordering::SeqCst);
                kill_process_tree(pid);
            }
        })
    };

    let status = child.wait();
    finished.store(true, Ordering::SeqCst);
    let _ = watchdog.join();

    let status = status.map_err(|e| format!("failed to wait for '{program}': {e}"))?;
    let stdout_data = stdout_handle.map(join_capture).unwrap_or_default();
    let stderr_data = stderr_handle.map(join_capture).unwrap_or_default();

    Ok(ProcessResultDto {
        program,
        args,
        cwd: cwd.to_string_lossy().into_owned(),
        stdout: stdout_data,
        stderr: stderr_data,
        status_code: status.code(),
        timed_out: timed_out.load(Ordering::SeqCst),
        duration_ms: started.elapsed().as_millis() as u64,
    })
}

/// If `program` is a bare name (no path separators) and a matching executable
/// exists directly in `cwd`, return its absolute path; otherwise return the
/// program unchanged (so PATH lookup still works for tools like `cargo`).
fn resolve_program_in_cwd(cwd: &Path, program: &str) -> std::ffi::OsString {
    if program.contains('/') || program.contains('\\') {
        return program.into();
    }
    let direct = cwd.join(program);
    if direct.is_file() {
        return direct.into_os_string();
    }
    #[cfg(windows)]
    {
        // Try common Windows executable extensions for a bare name.
        for ext in ["exe", "cmd", "bat", "com"] {
            let candidate = cwd.join(format!("{program}.{ext}"));
            if candidate.is_file() {
                return candidate.into_os_string();
            }
        }
    }
    program.into()
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

fn resolve_cwd(root_canon: &Path, relative_cwd: Option<&str>) -> Result<PathBuf, String> {
    let Some(rel) = relative_cwd.map(str::trim).filter(|s| !s.is_empty()) else {
        return Ok(root_canon.to_path_buf());
    };
    let candidate = root_canon.join(rel);
    let cwd_canon = candidate
        .canonicalize()
        .map_err(|e| format!("relative_cwd: {e}"))?;
    if !cwd_canon.starts_with(root_canon) {
        return Err(String::from("relative_cwd is outside the workspace"));
    }
    let meta = std::fs::metadata(&cwd_canon).map_err(|e| e.to_string())?;
    if !meta.is_dir() {
        return Err(String::from("relative_cwd is not a directory"));
    }
    Ok(cwd_canon)
}

fn spawn_capture<R: Read + Send + 'static>(mut reader: R) -> thread::JoinHandle<String> {
    thread::spawn(move || {
        let mut buf = Vec::new();
        let mut chunk = [0_u8; 8192];
        loop {
            match reader.read(&mut chunk) {
                Ok(0) => break,
                Ok(n) => {
                    if buf.len() < MAX_STREAM_BYTES {
                        let room = MAX_STREAM_BYTES - buf.len();
                        buf.extend_from_slice(&chunk[..n.min(room)]);
                    }
                }
                Err(_) => break,
            }
        }
        String::from_utf8_lossy(&buf).into_owned()
    })
}

fn join_capture(handle: thread::JoinHandle<String>) -> String {
    handle.join().unwrap_or_default()
}

#[cfg(windows)]
fn kill_process_tree(pid: u32) {
    let _ = command("taskkill")
        .args(["/PID", &pid.to_string(), "/T", "/F"])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status();
}

#[cfg(not(windows))]
fn kill_process_tree(pid: u32) {
    let _ = command("kill")
        .args(["-9", &pid.to_string()])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rejects_empty_program() {
        let temp = tempfile::tempdir().expect("temp workspace");
        let err = run_workspace_command(
            temp.path().to_string_lossy().into_owned(),
            String::from("  "),
            None,
            None,
            None,
        )
        .expect_err("empty program should fail");
        assert_eq!(err, "program must not be empty");
    }

    #[test]
    fn rejects_cwd_outside_workspace() {
        let temp = tempfile::tempdir().expect("temp workspace");
        let err = run_workspace_command(
            temp.path().to_string_lossy().into_owned(),
            String::from("rustc"),
            None,
            Some(String::from("../escape")),
            None,
        )
        .expect_err("escaping cwd should fail");
        assert!(err.starts_with("relative_cwd"), "got: {err}");
    }

    #[test]
    fn resolves_bare_program_name_living_in_cwd() {
        let temp = tempfile::tempdir().expect("temp workspace");
        let exe = if cfg!(windows) { "tool.exe" } else { "tool" };
        std::fs::write(temp.path().join(exe), b"stub").expect("wrote stub exe");

        let resolved = resolve_program_in_cwd(temp.path(), exe);
        assert_eq!(resolved, temp.path().join(exe).into_os_string());
    }

    #[test]
    fn leaves_path_tools_untouched() {
        let temp = tempfile::tempdir().expect("temp workspace");
        // `cargo` is not in cwd → resolver must return it unchanged for PATH lookup.
        let resolved = resolve_program_in_cwd(temp.path(), "cargo");
        assert_eq!(resolved, std::ffi::OsString::from("cargo"));
    }

    #[test]
    fn reports_unknown_program() {
        let temp = tempfile::tempdir().expect("temp workspace");
        let err = run_workspace_command(
            temp.path().to_string_lossy().into_owned(),
            String::from("definitely-not-a-real-program-xyz"),
            None,
            None,
            Some(5_000),
        )
        .expect_err("missing program should fail");
        assert!(err.starts_with("failed to start"), "got: {err}");
    }

    #[test]
    fn captures_stdout_and_exit_code() {
        let temp = tempfile::tempdir().expect("temp workspace");
        // rustc --version: always present in this toolchain, no source/linker needed.
        let result = run_workspace_command(
            temp.path().to_string_lossy().into_owned(),
            String::from("rustc"),
            Some(vec![String::from("--version")]),
            None,
            Some(30_000),
        )
        .expect("rustc --version runs");
        assert_eq!(result.status_code, Some(0));
        assert!(result.stdout.contains("rustc"), "stdout: {}", result.stdout);
        assert!(!result.timed_out);
    }

    #[test]
    fn enforces_timeout() {
        // `rustc -` reads source from stdin; we gave it a null stdin, but to get a
        // reliably long-running process we sleep via the platform shell.
        let temp = tempfile::tempdir().expect("temp workspace");
        let (program, args) = if cfg!(windows) {
            (
                String::from("cmd"),
                vec![
                    String::from("/C"),
                    String::from("ping -n 30 127.0.0.1 >NUL"),
                ],
            )
        } else {
            (String::from("sleep"), vec![String::from("30")])
        };
        let result = run_workspace_command(
            temp.path().to_string_lossy().into_owned(),
            program,
            Some(args),
            None,
            Some(1_000),
        )
        .expect("spawns and is killed");
        assert!(result.timed_out, "expected timeout, got: {result:?}",);
    }

    #[test]
    fn builds_and_runs_a_binary_end_to_end() {
        // rustc needs a system linker (cc/link.exe). If none is installed in this
        // environment, the *build* command still runs and returns a non-zero exit
        // with a linker error — which is itself a valid runner result. We assert
        // the happy path only when a binary is actually produced.
        let temp = tempfile::tempdir().expect("temp workspace");
        let src = temp.path().join("hello.rs");
        std::fs::write(&src, "fn main() { println!(\"cremniy-build-ok\"); }")
            .expect("wrote source");

        let exe_name = if cfg!(windows) { "hello.exe" } else { "hello" };
        let build = run_workspace_command(
            temp.path().to_string_lossy().into_owned(),
            String::from("rustc"),
            Some(vec![
                String::from("hello.rs"),
                String::from("-o"),
                exe_name.to_string(),
            ]),
            None,
            Some(120_000),
        )
        .expect("rustc build command runs");

        if !temp.path().join(exe_name).exists() {
            // No linker in this environment — runner worked, build couldn't link.
            assert_ne!(build.status_code, Some(0));
            return;
        }

        // Bare name: the runner resolves it against the workspace cwd on every
        // OS (this is the build-then-run ergonomics we want for agents).
        let run = run_workspace_command(
            temp.path().to_string_lossy().into_owned(),
            exe_name.to_string(),
            None,
            None,
            Some(30_000),
        )
        .expect("built binary runs");
        assert_eq!(run.status_code, Some(0));
        assert!(
            run.stdout.contains("cremniy-build-ok"),
            "binary stdout: {}",
            run.stdout
        );
    }
}

impl std::fmt::Debug for ProcessResultDto {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("ProcessResultDto")
            .field("program", &self.program)
            .field("status_code", &self.status_code)
            .field("timed_out", &self.timed_out)
            .field("duration_ms", &self.duration_ms)
            .finish()
    }
}
