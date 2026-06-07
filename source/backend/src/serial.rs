//! Serial terminal sessions for the "Connections" pack.
//!
//! Modelled on `terminal.rs` (the PTY engine): `SerialSessions` is Tauri-
//! managed state keyed by `sessionId`, each open port gets a reader thread
//! that streams bytes to the frontend as the `"serial://output"` event, and a
//! per-session stop flag lets us tear the reader down cleanly on close.
//!
//! `serialport` is cross-platform and pulls in no external tool — it talks to
//! the OS serial APIs directly (Win32 / termios / IOKit), which fits Cremniy's
//! zero-system-deps rule.

use std::collections::HashMap;
use std::io::Write;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use serde::Serialize;
use serialport::{DataBits, Parity, SerialPort, StopBits};
use tauri::{AppHandle, Emitter, State};

const SERIAL_OUTPUT_EVENT: &str = "serial://output";
const READ_BUFFER_SIZE: usize = 4096;
/// Short read timeout so the reader thread wakes often enough to notice the
/// stop flag (and so a quiet port doesn't block teardown).
const READ_TIMEOUT: Duration = Duration::from_millis(100);

#[derive(Default)]
pub struct SerialSessions {
    sessions: Arc<Mutex<HashMap<String, SerialSession>>>,
}

impl Drop for SerialSessions {
    fn drop(&mut self) {
        let Ok(mut sessions) = self.sessions.lock() else {
            return;
        };
        for (_, session) in sessions.drain() {
            session.stop.store(true, Ordering::Relaxed);
        }
    }
}

struct SerialSession {
    /// Write handle for the open port. The reader thread owns its own cloned
    /// handle, so writes and reads don't contend on the same lock.
    port: Arc<Mutex<Box<dyn SerialPort>>>,
    /// Set to signal the reader thread to exit; dropping the port closes it.
    stop: Arc<AtomicBool>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SerialPortInfo {
    name: String,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct SerialOutputDto {
    session_id: String,
    data: String,
}

/// List the serial ports the OS currently sees (COMx on Windows,
/// /dev/tty* on Unix).
#[tauri::command]
pub fn serial_ports() -> Result<Vec<SerialPortInfo>, String> {
    let ports = serialport::available_ports().map_err(|e| format!("list serial ports: {e}"))?;
    Ok(ports
        .into_iter()
        .map(|p| SerialPortInfo { name: p.port_name })
        .collect())
}

/// Open `port` at `baud` (8N1), register the session, and spawn a reader
/// thread that streams incoming bytes to the frontend.
#[tauri::command]
pub fn serial_open(
    app: AppHandle,
    state: State<'_, SerialSessions>,
    session_id: String,
    port: String,
    baud: u32,
) -> Result<(), String> {
    let session_id = session_id.trim().to_string();
    if session_id.is_empty() {
        return Err(String::from("session_id must not be empty"));
    }
    let port_name = port.trim();
    if port_name.is_empty() {
        return Err(String::from("port must not be empty"));
    }

    let port = serialport::new(port_name, baud)
        .data_bits(DataBits::Eight)
        .parity(Parity::None)
        .stop_bits(StopBits::One)
        .timeout(READ_TIMEOUT)
        .open()
        .map_err(|e| format!("open serial port {port_name}: {e}"))?;

    // Clone a reader handle so the reader thread and `serial_write` use
    // independent handles to the same port.
    let mut reader = port
        .try_clone()
        .map_err(|e| format!("clone serial port: {e}"))?;

    let stop = Arc::new(AtomicBool::new(false));
    let port = Arc::new(Mutex::new(port));

    {
        let mut guard = state
            .sessions
            .lock()
            .map_err(|_| String::from("serial session state is unavailable"))?;
        guard.insert(
            session_id.clone(),
            SerialSession {
                port: Arc::clone(&port),
                stop: Arc::clone(&stop),
            },
        );
    }

    // Reader thread — stream incoming bytes to the frontend until the port
    // closes or the stop flag is set. A read timeout surfaces as a `TimedOut`
    // error every `READ_TIMEOUT`; we treat that as "nothing to read, check the
    // stop flag and loop", not a fatal error.
    let app_for_reader = app.clone();
    let session_for_reader = session_id.clone();
    let stop_for_reader = Arc::clone(&stop);
    thread::spawn(move || {
        let mut buffer = [0_u8; READ_BUFFER_SIZE];
        loop {
            if stop_for_reader.load(Ordering::Relaxed) {
                break;
            }
            match reader.read(&mut buffer) {
                Ok(0) => continue,
                Ok(n) => {
                    let data = String::from_utf8_lossy(&buffer[..n]).into_owned();
                    let _ = app_for_reader.emit(
                        SERIAL_OUTPUT_EVENT,
                        SerialOutputDto {
                            session_id: session_for_reader.clone(),
                            data,
                        },
                    );
                }
                Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => continue,
                Err(_) => break,
            }
        }
    });

    Ok(())
}

/// Write bytes to the open port.
#[tauri::command]
pub fn serial_write(
    state: State<'_, SerialSessions>,
    session_id: String,
    data: String,
) -> Result<(), String> {
    let port = {
        let guard = state
            .sessions
            .lock()
            .map_err(|_| String::from("serial session state is unavailable"))?;
        guard
            .get(session_id.trim())
            .map(|s| Arc::clone(&s.port))
            .ok_or_else(|| String::from("serial session not found"))?
    };
    let mut port = port
        .lock()
        .map_err(|_| String::from("serial port is unavailable"))?;
    port.write_all(data.as_bytes())
        .map_err(|e| format!("write serial: {e}"))?;
    port.flush().map_err(|e| format!("flush serial: {e}"))
}

/// Stop the reader thread and drop the port (which closes it).
#[tauri::command]
pub fn serial_close(state: State<'_, SerialSessions>, session_id: String) -> Result<(), String> {
    let session = {
        let mut guard = state
            .sessions
            .lock()
            .map_err(|_| String::from("serial session state is unavailable"))?;
        guard.remove(session_id.trim())
    };
    if let Some(session) = session {
        session.stop.store(true, Ordering::Relaxed);
        // Dropping `session` drops the last `Arc` we hold to the port; once the
        // reader thread's cloned handle also drops, the OS closes the port.
    }
    Ok(())
}
