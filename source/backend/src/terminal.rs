use std::collections::HashMap;
use std::io::{Read, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, Command, ExitStatus, Stdio};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use tauri::{AppHandle, Emitter, State};

const TERMINAL_OUTPUT_EVENT: &str = "terminal://output";
const READ_BUFFER_SIZE: usize = 8192;
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
            let Ok(mut child) = session.child.lock() else {
                continue;
            };
            let _ = terminate_child(&mut child);
        }
    }
}

struct TerminalSession {
    child: Arc<Mutex<Child>>,
    stdin: ChildStdin,
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
    let shell = default_shell_path();
    let session_id = sessions.next_session_id();
    let mut command = build_shell_command(&shell, &cwd);

    let mut child = command.spawn().map_err(|e| format!("start shell: {e}"))?;
    let stdin = match child.stdin.take() {
        Some(stdin) => stdin,
        None => {
            return Err(cleanup_spawned_child(
                &mut child,
                "shell stdin is unavailable",
            ))
        }
    };
    let stdout = match child.stdout.take() {
        Some(stdout) => stdout,
        None => {
            return Err(cleanup_spawned_child(
                &mut child,
                "shell stdout is unavailable",
            ))
        }
    };
    let stderr = match child.stderr.take() {
        Some(stderr) => stderr,
        None => {
            return Err(cleanup_spawned_child(
                &mut child,
                "shell stderr is unavailable",
            ))
        }
    };
    let child = Arc::new(Mutex::new(child));

    let mut guard = match sessions.sessions.lock() {
        Ok(guard) => guard,
        Err(_) => {
            cleanup_child_arc(&child);
            return Err(String::from("terminal session state is unavailable"));
        }
    };
    guard.insert(
        session_id.clone(),
        TerminalSession {
            child: Arc::clone(&child),
            stdin,
        },
    );
    drop(guard);

    spawn_output_reader(
        app.clone(),
        session_id.clone(),
        TerminalOutputStream::Stdout,
        stdout,
    );
    spawn_output_reader(
        app.clone(),
        session_id.clone(),
        TerminalOutputStream::Stderr,
        stderr,
    );
    spawn_exit_monitor(
        app.clone(),
        Arc::clone(&sessions.sessions),
        session_id.clone(),
        child,
    );
    emit_system_message(
        &app,
        &session_id,
        &format!(
            "Terminal started without PTY support. Some interactive shell features may be limited. {}\n",
            process_termination_message()
        ),
    );

    Ok(TerminalStartDto {
        session_id,
        shell,
        cwd: cwd.to_string_lossy().into_owned(),
        supports_interrupt: false,
    })
}

#[tauri::command]
pub fn write_terminal_input(
    sessions: State<'_, TerminalSessions>,
    session_id: String,
    input: String,
) -> Result<(), String> {
    let mut guard = sessions
        .sessions
        .lock()
        .map_err(|_| String::from("terminal session state is unavailable"))?;
    let session = guard
        .get_mut(session_id.trim())
        .ok_or_else(|| String::from("terminal session not found or has exited"))?;

    {
        let mut child = session
            .child
            .lock()
            .map_err(|_| String::from("terminal session process state is unavailable"))?;
        if child
            .try_wait()
            .map_err(|e| format!("check shell status: {e}"))?
            .is_some()
        {
            return Err(String::from("terminal session has exited"));
        }
    }

    session
        .stdin
        .write_all(input.as_bytes())
        .map_err(|e| format!("write shell input: {e}"))?;
    session
        .stdin
        .flush()
        .map_err(|e| format!("flush shell input: {e}"))
}

#[tauri::command]
pub fn stop_terminal_session(
    sessions: State<'_, TerminalSessions>,
    session_id: String,
) -> Result<(), String> {
    let mut guard = sessions
        .sessions
        .lock()
        .map_err(|_| String::from("terminal session state is unavailable"))?;
    let Some(session) = guard.remove(session_id.trim()) else {
        return Ok(());
    };

    let mut child = session
        .child
        .lock()
        .map_err(|_| String::from("terminal session process state is unavailable"))?;
    terminate_child(&mut child)
}

#[tauri::command]
pub fn interrupt_terminal_session(
    _sessions: State<'_, TerminalSessions>,
    _session_id: String,
) -> Result<(), String> {
    Err(String::from(
        "terminal interrupt is not supported by the current non-PTY process bridge",
    ))
}

#[tauri::command]
pub fn get_terminal_capabilities() -> TerminalCapabilityDto {
    TerminalCapabilityDto {
        supports_interrupt: false,
        reason: terminal_capability_reason(),
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

fn default_shell_path() -> String {
    #[cfg(windows)]
    {
        String::from("powershell.exe")
    }

    #[cfg(not(windows))]
    {
        env_non_empty("SHELL").unwrap_or_else(|| String::from("/bin/sh"))
    }
}

#[cfg(not(windows))]
fn env_non_empty(key: &str) -> Option<String> {
    std::env::var(key)
        .ok()
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty())
}

fn build_shell_command(shell: &str, cwd: &Path) -> Command {
    let mut command = Command::new(shell);
    add_shell_args(&mut command, shell);
    command
        .current_dir(cwd)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());
    command
}

fn cleanup_spawned_child(child: &mut Child, error: &str) -> String {
    let _ = terminate_child(child);
    String::from(error)
}

fn cleanup_child_arc(child: &Arc<Mutex<Child>>) {
    if let Ok(mut child) = child.lock() {
        let _ = terminate_child(&mut child);
    }
}

fn terminate_child(child: &mut Child) -> Result<(), String> {
    match child.try_wait() {
        Ok(Some(_)) => Ok(()),
        Ok(None) => {
            terminate_running_child(child)?;
            let _ = child.wait();
            Ok(())
        }
        Err(e) => Err(format!("check shell status: {e}")),
    }
}

#[cfg(windows)]
fn terminate_running_child(child: &mut Child) -> Result<(), String> {
    // `Child::kill` only terminates the shell. On Windows, taskkill is available
    // by default and can terminate descendants without adding a fragile dependency.
    let pid = child.id().to_string();
    let status = Command::new("taskkill")
        .args(["/PID", &pid, "/T", "/F"])
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status();

    if matches!(status, Ok(status) if status.success()) {
        return Ok(());
    }
    if matches!(child.try_wait(), Ok(Some(_))) {
        return Ok(());
    }
    child.kill().map_err(|e| format!("terminate shell: {e}"))
}

#[cfg(not(windows))]
fn terminate_running_child(child: &mut Child) -> Result<(), String> {
    // Without a PTY/process group owner, std::process can reliably kill only
    // the direct shell process. Surface that limitation in capabilities/UI text.
    child.kill().map_err(|e| format!("terminate shell: {e}"))
}

fn terminal_capability_reason() -> String {
    format!(
        "The current bridge uses std::process pipes instead of a PTY. {}",
        process_termination_message()
    )
}

#[cfg(windows)]
fn process_termination_message() -> &'static str {
    "Stopping a session uses best-effort Windows process-tree termination."
}

#[cfg(not(windows))]
fn process_termination_message() -> &'static str {
    "Stopping a session terminates only the shell process, not its full process tree."
}

fn add_shell_args(command: &mut Command, shell: &str) {
    #[cfg(windows)]
    {
        if shell.eq_ignore_ascii_case("powershell.exe") {
            command.args(["-NoLogo", "-NoExit"]);
        }
    }

    #[cfg(not(windows))]
    {
        if shell.ends_with("/bash") || shell == "bash" {
            command.arg("-i");
        }
    }
}

fn spawn_output_reader<R>(
    app: AppHandle,
    session_id: String,
    stream: TerminalOutputStream,
    mut reader: R,
) where
    R: Read + Send + 'static,
{
    thread::spawn(move || {
        let mut buffer = [0_u8; READ_BUFFER_SIZE];
        loop {
            match reader.read(&mut buffer) {
                Ok(0) => break,
                Ok(bytes_read) => {
                    let data = String::from_utf8_lossy(&buffer[..bytes_read]).into_owned();
                    let _ = app.emit(
                        TERMINAL_OUTPUT_EVENT,
                        TerminalOutputDto {
                            session_id: session_id.clone(),
                            stream: stream.clone(),
                            data,
                        },
                    );
                }
                Err(e) => {
                    let _ = app.emit(
                        TERMINAL_OUTPUT_EVENT,
                        TerminalOutputDto {
                            session_id: session_id.clone(),
                            stream: TerminalOutputStream::System,
                            data: format!("Terminal stream error: {e}\n"),
                        },
                    );
                    break;
                }
            }
        }
    });
}

fn spawn_exit_monitor(
    app: AppHandle,
    sessions: Arc<Mutex<HashMap<String, TerminalSession>>>,
    session_id: String,
    child: Arc<Mutex<Child>>,
) {
    thread::spawn(move || loop {
        thread::sleep(EXIT_POLL_INTERVAL);

        let exit_status = {
            let mut child = match child.lock() {
                Ok(child) => child,
                Err(_) => {
                    emit_system_message(
                        &app,
                        &session_id,
                        "Terminal process state is unavailable.\n",
                    );
                    return;
                }
            };

            match child.try_wait() {
                Ok(Some(status)) => Some(status),
                Ok(None) => None,
                Err(e) => {
                    emit_system_message(
                        &app,
                        &session_id,
                        &format!("Terminal status check failed: {e}\n"),
                    );
                    return;
                }
            }
        };

        if let Some(status) = exit_status {
            if let Ok(mut sessions) = sessions.lock() {
                sessions.remove(&session_id);
            }
            emit_terminal_exit(&app, &session_id, status);
            return;
        }
    });
}

fn emit_system_message(app: &AppHandle, session_id: &str, data: &str) {
    let _ = app.emit(
        TERMINAL_OUTPUT_EVENT,
        TerminalOutputDto {
            session_id: session_id.to_string(),
            stream: TerminalOutputStream::System,
            data: data.to_string(),
        },
    );
}

fn emit_terminal_exit(app: &AppHandle, session_id: &str, status: ExitStatus) {
    let _ = app.emit(
        TERMINAL_OUTPUT_EVENT,
        TerminalOutputDto {
            session_id: session_id.to_string(),
            stream: TerminalOutputStream::Exit,
            data: format!("Terminal exited ({status}).\n"),
        },
    );
}

#[cfg(test)]
mod tests {
    use super::{
        canonical_workspace_directory, default_shell_path, get_terminal_capabilities,
        terminal_capability_reason, TerminalOutputDto, TerminalOutputStream, TerminalStartDto,
    };
    use serde_json::json;

    #[test]
    fn canonical_workspace_directory_rejects_empty_path() {
        assert_eq!(
            canonical_workspace_directory("").unwrap_err(),
            "workspace_root must not be empty"
        );
    }

    #[test]
    fn canonical_workspace_directory_accepts_existing_directory() {
        let cwd = std::env::current_dir().expect("test cwd");
        let resolved =
            canonical_workspace_directory(cwd.to_string_lossy().as_ref()).expect("resolved cwd");
        assert!(resolved.is_dir());
    }

    #[test]
    fn terminal_start_dto_serializes_frontend_contract() {
        let dto = TerminalStartDto {
            session_id: String::from("terminal-1"),
            shell: String::from("powershell.exe"),
            cwd: String::from("C:\\work"),
            supports_interrupt: false,
        };

        assert_eq!(
            serde_json::to_value(dto).expect("serialized terminal start dto"),
            json!({
                "sessionId": "terminal-1",
                "shell": "powershell.exe",
                "cwd": "C:\\work",
                "supportsInterrupt": false,
            })
        );
    }

    #[test]
    fn terminal_output_dto_serializes_streams_as_domain_values() {
        let cases = [
            (TerminalOutputStream::Stdout, "stdout"),
            (TerminalOutputStream::Stderr, "stderr"),
            (TerminalOutputStream::System, "system"),
            (TerminalOutputStream::Exit, "exit"),
        ];

        for (stream, expected_stream) in cases {
            let dto = TerminalOutputDto {
                session_id: String::from("terminal-1"),
                stream,
                data: String::from("output"),
            };

            assert_eq!(
                serde_json::to_value(dto).expect("serialized terminal output dto"),
                json!({
                    "sessionId": "terminal-1",
                    "stream": expected_stream,
                    "data": "output",
                })
            );
        }
    }

    #[test]
    fn terminal_capabilities_serialize_unsupported_interrupt_contract() {
        assert_eq!(
            serde_json::to_value(get_terminal_capabilities())
                .expect("serialized terminal capabilities"),
            json!({
                "supportsInterrupt": false,
                "reason": terminal_capability_reason(),
            })
        );
    }

    #[cfg(windows)]
    #[test]
    fn default_shell_path_uses_fixed_windows_shell() {
        assert_eq!(default_shell_path(), "powershell.exe");
    }

    #[cfg(not(windows))]
    #[test]
    fn default_shell_path_uses_shell_environment_or_sh() {
        assert!(!default_shell_path().trim().is_empty());
    }
}
