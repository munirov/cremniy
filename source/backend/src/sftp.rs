//! SFTP (file transfer over SSH) sessions for the "Connections" pack.
//!
//! Modelled on `ssh.rs` (the interactive SSH engine): connect with `russh`,
//! authenticate, open a channel — but instead of requesting a shell we request
//! the `sftp` subsystem and wrap the channel's byte stream in a
//! `russh_sftp::client::SftpSession`. `SftpSessions` is Tauri-managed state
//! keyed by `sessionId`.
//!
//! Like `ssh.rs`, the moving parts are not `Sync`: the channel stream behind the
//! `SftpSession` can't be shared across tasks freely. `ssh.rs` solved this by
//! owning the channel in one tokio task; here the lifetime is request/response
//! (list / read / write), so instead each session lives behind a
//! `tokio::sync::Mutex` and a command locks it for the duration of one op. The
//! SSH connection `Handle` is kept alive alongside the session (dropping it
//! would tear down the channel the SFTP session rides on).

use std::collections::HashMap;
use std::sync::{Arc, Mutex};

use russh::client::{self, Handle};
use russh::keys::key::PublicKey;
use russh_sftp::client::SftpSession;
use serde::Serialize;
use tauri::{AppHandle, State};
use tokio::sync::Mutex as AsyncMutex;

#[derive(Default)]
pub struct SftpSessions {
    /// The outer `std::sync::Mutex` guards the map; each session sits behind its
    /// own async mutex so one in-flight transfer doesn't block listing another
    /// session. `Arc` lets a command clone the handle out and release the map
    /// lock before awaiting the (non-`Sync`) SFTP session.
    sessions: Arc<Mutex<HashMap<String, Arc<AsyncMutex<SftpConn>>>>>,
}

struct SftpConn {
    sftp: SftpSession,
    /// Keep the SSH connection alive for as long as the SFTP session runs —
    /// dropping it closes the channel the SFTP subsystem rides on.
    _handle: Handle<ClientHandler>,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
pub struct SftpEntry {
    name: String,
    path: String,
    is_dir: bool,
    size: u64,
}

/// Minimal client handler — accepts any host key for now (same TODO as ssh.rs:
/// wire host-key verification against a known_hosts store later).
struct ClientHandler;

#[async_trait::async_trait]
impl client::Handler for ClientHandler {
    type Error = russh::Error;

    async fn check_server_key(
        &mut self,
        _server_public_key: &PublicKey,
    ) -> Result<bool, Self::Error> {
        Ok(true)
    }
}

/// Connect to `address:port`, authenticate `username` with `password`, open the
/// SFTP subsystem, and register the session under `session_id`.
#[tauri::command]
pub async fn sftp_open(
    _app: AppHandle,
    state: State<'_, SftpSessions>,
    session_id: String,
    address: String,
    port: u16,
    username: String,
    password: Option<String>,
) -> Result<(), String> {
    let session_id = session_id.trim().to_string();
    if session_id.is_empty() {
        return Err(String::from("session_id must not be empty"));
    }
    let address = address.trim().to_string();
    if address.is_empty() {
        return Err(String::from("address must not be empty"));
    }
    let username = username.trim().to_string();
    if username.is_empty() {
        return Err(String::from("username must not be empty"));
    }

    let config = Arc::new(client::Config::default());

    let mut handle: Handle<ClientHandler> =
        client::connect(config, (address.as_str(), port), ClientHandler)
            .await
            .map_err(|e| format!("connect to {address}:{port}: {e}"))?;

    // Authenticate. With a password we use password auth; without one we try
    // the "none" method (some servers allow it) and otherwise fail clearly.
    let authenticated = match password {
        Some(ref pw) => handle
            .authenticate_password(&username, pw)
            .await
            .map_err(|e| format!("authenticate {username}@{address}: {e}"))?,
        None => handle
            .authenticate_none(&username)
            .await
            .map_err(|e| format!("authenticate {username}@{address}: {e}"))?,
    };
    if !authenticated {
        return Err(if password.is_some() {
            format!("authentication failed for {username}@{address} (wrong password?)")
        } else {
            format!("authentication failed for {username}@{address} (no password supplied; key/agent auth isn't wired yet)")
        });
    }

    let channel = handle
        .channel_open_session()
        .await
        .map_err(|e| format!("open ssh channel: {e}"))?;
    channel
        .request_subsystem(true, "sftp")
        .await
        .map_err(|e| format!("request sftp subsystem: {e}"))?;
    let sftp = SftpSession::new(channel.into_stream())
        .await
        .map_err(|e| format!("start sftp session: {e}"))?;

    {
        let mut guard = state
            .sessions
            .lock()
            .map_err(|_| String::from("sftp session state is unavailable"))?;
        guard.insert(
            session_id,
            Arc::new(AsyncMutex::new(SftpConn {
                sftp,
                _handle: handle,
            })),
        );
    }

    Ok(())
}

/// List the entries of `path` on the remote host. Directories sort before
/// files, then by name (case-insensitive) — same ordering as the local
/// `list_directory` in lib.rs.
#[tauri::command]
pub async fn sftp_list(
    state: State<'_, SftpSessions>,
    session_id: String,
    path: String,
) -> Result<Vec<SftpEntry>, String> {
    let conn = session_handle(&state, &session_id)?;
    let path = path.trim();
    let path = if path.is_empty() { "." } else { path };

    let guard = conn.lock().await;
    let read_dir = guard
        .sftp
        .read_dir(path)
        .await
        .map_err(|e| format!("list {path}: {e}"))?;

    let mut entries: Vec<SftpEntry> = read_dir
        .map(|entry| {
            let meta = entry.metadata();
            SftpEntry {
                name: entry.file_name(),
                path: entry.path(),
                is_dir: meta.is_dir(),
                size: meta.len(),
            }
        })
        .collect();
    entries.sort_by(|a, b| match (a.is_dir, b.is_dir) {
        (true, false) => std::cmp::Ordering::Less,
        (false, true) => std::cmp::Ordering::Greater,
        _ => a.name.to_lowercase().cmp(&b.name.to_lowercase()),
    });
    Ok(entries)
}

/// Download the remote file at `path` into memory (the frontend writes it to a
/// local path).
#[tauri::command]
pub async fn sftp_read(
    state: State<'_, SftpSessions>,
    session_id: String,
    path: String,
) -> Result<Vec<u8>, String> {
    let conn = session_handle(&state, &session_id)?;
    let path = path.trim().to_string();
    if path.is_empty() {
        return Err(String::from("path must not be empty"));
    }
    let guard = conn.lock().await;
    guard
        .sftp
        .read(path.as_str())
        .await
        .map_err(|e| format!("read {path}: {e}"))
}

/// Upload `data` to the remote file at `path` (creating / truncating it).
#[tauri::command]
pub async fn sftp_write(
    state: State<'_, SftpSessions>,
    session_id: String,
    path: String,
    data: Vec<u8>,
) -> Result<(), String> {
    let conn = session_handle(&state, &session_id)?;
    let path = path.trim().to_string();
    if path.is_empty() {
        return Err(String::from("path must not be empty"));
    }
    let guard = conn.lock().await;
    guard
        .sftp
        .write(path.as_str(), &data)
        .await
        .map_err(|e| format!("write {path}: {e}"))
}

/// Close the SFTP session: remove it from the map and drop it (which drops the
/// SSH connection handle, closing the channel).
#[tauri::command]
pub async fn sftp_close(state: State<'_, SftpSessions>, session_id: String) -> Result<(), String> {
    let conn = {
        let mut guard = state
            .sessions
            .lock()
            .map_err(|_| String::from("sftp session state is unavailable"))?;
        guard.remove(session_id.trim())
    };
    // Politely close the SFTP session before dropping (best-effort).
    if let Some(conn) = conn {
        let guard = conn.lock().await;
        let _ = guard.sftp.close().await;
    }
    Ok(())
}

/// Clone the `Arc` handle for `session_id` out of the map, releasing the map
/// lock before the caller awaits the (non-`Sync`) session behind it.
fn session_handle(
    state: &State<'_, SftpSessions>,
    session_id: &str,
) -> Result<Arc<AsyncMutex<SftpConn>>, String> {
    let guard = state
        .sessions
        .lock()
        .map_err(|_| String::from("sftp session state is unavailable"))?;
    guard
        .get(session_id.trim())
        .cloned()
        .ok_or_else(|| String::from("sftp session not found"))
}
