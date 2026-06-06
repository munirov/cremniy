//! Real PTY terminal sessions (Qt parity: TerminalWidget).
//!
//! `portable-pty` opens a ConPTY on Windows 10+ and a POSIX PTY on
//! Linux/macOS. Each session gets a master/slave pair: the shell runs in the
//! slave (with a real TTY), and we read/write the master from the GUI. With a
//! real PTY we get echo, line editing, prompts, ANSI escapes, `vim`/`less`
//! interactivity, and — most importantly — Ctrl+C / Ctrl+D delivered as
//! signals to the foreground process group, not to the shell.

use std::collections::HashMap;
use std::io::{Read, Write};
use std::path::PathBuf;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use portable_pty::{CommandBuilder, NativePtySystem, PtySize, PtySystem};
use tauri::{AppHandle, Emitter, State};

const TERMINAL_OUTPUT_EVENT: &str = "terminal://output";
const READ_BUFFER_SIZE: usize = 4096;
const EXIT_POLL_INTERVAL: Duration = Duration::from_millis(250);

#[derive(Default)]
pub struct TerminalSessions {
    next_id: AtomicU64,
    sessions: Arc<Mutex<HashMap<String, TerminalSession>>>,
}

impl Drop for TerminalSessions {
    fn drop(&mut self) {
        let Ok(mut sessions) = self.sessions.lock() else {
            return;
        };
        for (_, session) in sessions.drain() {
            if let Ok(mut child) = session.child.lock() {
                let _ = child.kill();
            }
        }
    }
}

struct TerminalSession {
    master: Arc<Mutex<Box<dyn portable_pty::MasterPty + Send>>>,
    writer: Arc<Mutex<Box<dyn Write + Send>>>,
    child: Arc<Mutex<Box<dyn portable_pty::Child + Send + Sync>>>,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct TerminalStartDto {
    session_id: String,
    shell: String,
    cwd: String,
    supports_interrupt: bool,
}

#[derive(serde::Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct TerminalOutputDto {
    session_id: String,
    stream: TerminalOutputStream,
    data: String,
}

#[derive(serde::Serialize, Clone)]
#[serde(rename_all = "camelCase")]
enum TerminalOutputStream {
    Stdout,
    Stderr,
    System,
    Exit,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct TerminalCapabilityDto {
    supports_interrupt: bool,
    reason: String,
}

#[tauri::command]
pub fn start_terminal_session(
    app: AppHandle,
    sessions: State<'_, TerminalSessions>,
    workspace_root: String,
) -> Result<TerminalStartDto, String> {
    let cwd = canonical_workspace_directory(&workspace_root)?;
    // cmd.exe / ConPTY don't understand the Windows `\\?\` extended-length
    // prefix that `canonicalize()` produces. Without stripping it the shell
    // silently falls back to %SystemRoot% (C:\Windows). Pass a plain path.
    let cwd_for_shell = strip_unc_prefix(cwd.clone());
    let shell = default_shell_path();
    let session_id = sessions.next_session_id();

    let pty_system = NativePtySystem::default();
    let pair = pty_system
        .openpty(PtySize {
            rows: 24,
            cols: 80,
            pixel_width: 0,
            pixel_height: 0,
        })
        .map_err(|e| format!("open pty: {e}"))?;

    let mut cmd = CommandBuilder::new(&shell);
    cmd.cwd(&cwd_for_shell);

    // cmd.exe: append `$S` (a space) to PROMPT so typed input isn't glued to the
    // ">" of the prompt — i.e. `…test-c> cmd` instead of `…test-c>cmd`. Keeps any
    // custom prompt the user already has; only adds the trailing space.
    if shell.to_lowercase().contains("cmd") {
        let mut prompt = std::env::var("PROMPT").unwrap_or_else(|_| String::from("$P$G"));
        if !prompt.to_lowercase().ends_with("$s") && !prompt.ends_with(' ') {
            prompt.push_str("$S");
        }
        cmd.env("PROMPT", prompt);
    }

    let child = pair
        .slave
        .spawn_command(cmd)
        .map_err(|e| format!("spawn shell: {e}"))?;
    drop(pair.slave);

    let mut reader = pair
        .master
        .try_clone_reader()
        .map_err(|e| format!("clone pty reader: {e}"))?;
    let writer = pair
        .master
        .take_writer()
        .map_err(|e| format!("take pty writer: {e}"))?;

    let master = Arc::new(Mutex::new(pair.master));
    let writer = Arc::new(Mutex::new(writer));
    let child = Arc::new(Mutex::new(child));

    {
        let mut guard = sessions
            .sessions
            .lock()
            .map_err(|_| String::from("terminal session state is unavailable"))?;
        guard.insert(
            session_id.clone(),
            TerminalSession {
                master: Arc::clone(&master),
                writer: Arc::clone(&writer),
                child: Arc::clone(&child),
            },
        );
    }

    // Reader thread — stream master output to the frontend as a single
    // 'stdout' stream (PTY merges stdout+stderr).
    let app_for_reader = app.clone();
    let session_for_reader = session_id.clone();
    thread::spawn(move || {
        let mut buffer = [0_u8; READ_BUFFER_SIZE];
        loop {
            match reader.read(&mut buffer) {
                Ok(0) => break,
                Ok(n) => {
                    let data = String::from_utf8_lossy(&buffer[..n]).into_owned();
                    let _ = app_for_reader.emit(
                        TERMINAL_OUTPUT_EVENT,
                        TerminalOutputDto {
                            session_id: session_for_reader.clone(),
                            stream: TerminalOutputStream::Stdout,
                            data,
                        },
                    );
                }
                Err(_) => break,
            }
        }
    });

    // Exit-monitor thread.
    let app_for_exit = app.clone();
    let sessions_for_exit = Arc::clone(&sessions.sessions);
    let session_for_exit = session_id.clone();
    let child_for_exit = Arc::clone(&child);
    thread::spawn(move || loop {
        thread::sleep(EXIT_POLL_INTERVAL);
        let mut guard = match child_for_exit.lock() {
            Ok(g) => g,
            Err(_) => return,
        };
        match guard.try_wait() {
            Ok(Some(status)) => {
                drop(guard);
                if let Ok(mut sessions) = sessions_for_exit.lock() {
                    sessions.remove(&session_for_exit);
                }
                let _ = app_for_exit.emit(
                    TERMINAL_OUTPUT_EVENT,
                    TerminalOutputDto {
                        session_id: session_for_exit.clone(),
                        stream: TerminalOutputStream::Exit,
                        data: format!("Terminal exited ({status:?}).\n"),
                    },
                );
                return;
            }
            Ok(None) => continue,
            Err(_) => return,
        }
    });

    Ok(TerminalStartDto {
        session_id,
        shell,
        cwd: cwd.to_string_lossy().into_owned(),
        supports_interrupt: true,
    })
}

#[tauri::command]
pub fn write_terminal_input(
    sessions: State<'_, TerminalSessions>,
    session_id: String,
    input: String,
) -> Result<(), String> {
    let writer = {
        let guard = sessions
            .sessions
            .lock()
            .map_err(|_| String::from("terminal session state is unavailable"))?;
        guard
            .get(session_id.trim())
            .map(|s| Arc::clone(&s.writer))
            .ok_or_else(|| String::from("terminal session not found"))?
    };
    let mut writer = writer
        .lock()
        .map_err(|_| String::from("terminal writer is unavailable"))?;
    writer
        .write_all(input.as_bytes())
        .map_err(|e| format!("write pty: {e}"))?;
    writer
        .flush()
        .map_err(|e| format!("flush pty: {e}"))
}

#[tauri::command]
pub fn resize_terminal_session(
    sessions: State<'_, TerminalSessions>,
    session_id: String,
    rows: u16,
    cols: u16,
) -> Result<(), String> {
    let master = {
        let guard = sessions
            .sessions
            .lock()
            .map_err(|_| String::from("terminal session state is unavailable"))?;
        guard
            .get(session_id.trim())
            .map(|s| Arc::clone(&s.master))
            .ok_or_else(|| String::from("terminal session not found"))?
    };
    let master = master
        .lock()
        .map_err(|_| String::from("terminal master is unavailable"))?;
    master
        .resize(PtySize {
            rows: rows.max(1),
            cols: cols.max(1),
            pixel_width: 0,
            pixel_height: 0,
        })
        .map_err(|e| format!("resize pty: {e}"))
}

#[tauri::command]
pub fn stop_terminal_session(
    sessions: State<'_, TerminalSessions>,
    session_id: String,
) -> Result<(), String> {
    let session = {
        let mut guard = sessions
            .sessions
            .lock()
            .map_err(|_| String::from("terminal session state is unavailable"))?;
        guard.remove(session_id.trim())
    };
    if let Some(session) = session {
        if let Ok(mut child) = session.child.lock() {
            let _ = child.kill();
        }
    }
    Ok(())
}

/// Send Ctrl+C (0x03) to the master. Inside a real PTY the kernel translates
/// this to SIGINT on the foreground process group — same as a hardware
/// terminal would do. cmd.exe / bash / python / vim all react correctly.
#[tauri::command]
pub fn interrupt_terminal_session(
    sessions: State<'_, TerminalSessions>,
    session_id: String,
) -> Result<(), String> {
    let writer = {
        let guard = sessions
            .sessions
            .lock()
            .map_err(|_| String::from("terminal session state is unavailable"))?;
        guard
            .get(session_id.trim())
            .map(|s| Arc::clone(&s.writer))
            .ok_or_else(|| String::from("terminal session not found"))?
    };
    let mut writer = writer
        .lock()
        .map_err(|_| String::from("terminal writer is unavailable"))?;
    writer
        .write_all(&[0x03])
        .map_err(|e| format!("write interrupt: {e}"))?;
    let _ = writer.flush();
    Ok(())
}

#[tauri::command]
pub fn get_terminal_capabilities() -> TerminalCapabilityDto {
    TerminalCapabilityDto {
        supports_interrupt: true,
        reason: "ConPTY / POSIX PTY: Ctrl+C is delivered as SIGINT to the foreground process group.".to_string(),
    }
}

impl TerminalSessions {
    fn next_session_id(&self) -> String {
        let id = self.next_id.fetch_add(1, Ordering::Relaxed) + 1;
        format!("terminal-{}-{id}", std::process::id())
    }
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

fn strip_unc_prefix(p: PathBuf) -> PathBuf {
    #[cfg(windows)]
    {
        let s = p.to_string_lossy();
        if let Some(stripped) = s.strip_prefix(r"\\?\") {
            if !stripped.starts_with("UNC\\") {
                return PathBuf::from(stripped);
            }
        }
    }
    p
}

fn default_shell_path() -> String {
    #[cfg(windows)]
    {
        // With a real PTY both cmd.exe and powershell.exe work beautifully —
        // they get a TTY, echo, prompts, and proper Ctrl+C handling. cmd is
        // lighter; PowerShell would also be fine. Pick cmd by default.
        String::from("cmd.exe")
    }

    #[cfg(not(windows))]
    {
        std::env::var("SHELL")
            .ok()
            .map(|v| v.trim().to_string())
            .filter(|v| !v.is_empty())
            .unwrap_or_else(|| String::from("/bin/sh"))
    }
}
