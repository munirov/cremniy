//! Interactive SSH shell sessions for the "Connections" pack.
//!
//! Modelled on `serial.rs` (the serial engine): `SshSessions` is Tauri-managed
//! state keyed by `sessionId`, each open shell gets a read loop that streams
//! bytes to the frontend as the `"ssh://output"` event, and closing the channel
//! tears the session down (emitting `"ssh://exit"`).
//!
//! Unlike serial (blocking OS handles + threads), SSH is async: `russh` is a
//! pure-Rust client that runs on the tokio runtime Tauri already exposes via
//! `tauri::async_runtime`. So instead of a stop flag + reader thread, a session
//! owns an `mpsc` channel feeding a writer task; dropping the sender ends the
//! writer task and (with the read loop) closes the channel.

use std::collections::HashMap;
use std::sync::{Arc, Mutex};

use russh::client::{self, Handle};
use russh::keys::key::PublicKey;
use russh::ChannelMsg;
use serde::Serialize;
use tauri::{AppHandle, Emitter, State};
use tokio::sync::mpsc::{self, UnboundedSender};

const SSH_OUTPUT_EVENT: &str = "ssh://output";
const SSH_EXIT_EVENT: &str = "ssh://exit";

#[derive(Default)]
pub struct SshSessions {
    sessions: Arc<Mutex<HashMap<String, SshHandle>>>,
}

struct SshHandle {
    /// Push stdin bytes to the channel's writer task. Dropping the last clone
    /// ends the writer task; combined with the read loop exiting, the channel
    /// closes and the SSH session winds down.
    stdin: UnboundedSender<Vec<u8>>,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct SshOutputDto {
    session_id: String,
    data: String,
}

#[derive(Serialize, Clone)]
#[serde(rename_all = "camelCase")]
struct SshExitDto {
    session_id: String,
}

/// Minimal client handler. We accept any host key for now.
struct ClientHandler;

#[async_trait::async_trait]
impl client::Handler for ClientHandler {
    type Error = russh::Error;

    // TODO: host-key verification — compare `server_public_key` against a
    // known_hosts store and prompt the user on first connect / mismatch.
    async fn check_server_key(
        &mut self,
        _server_public_key: &PublicKey,
    ) -> Result<bool, Self::Error> {
        Ok(true)
    }
}

/// Connect to `address:port`, authenticate `username` with `password`, open an
/// interactive shell, register the session, and spawn the read/write loops that
/// bridge the channel to the frontend (`ssh://output` / `ssh://exit`).
#[tauri::command]
pub async fn ssh_open(
    app: AppHandle,
    state: State<'_, SshSessions>,
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

    let mut handle: Handle<ClientHandler> = client::connect(config, (address.as_str(), port), ClientHandler)
        .await
        .map_err(|e| format!("connect to {address}:{port}: {e}"))?;

    // Authenticate. With a password we use password auth; without one we try the
    // "none" method (some servers allow it) and otherwise fail with a clear hint.
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

    let mut channel = handle
        .channel_open_session()
        .await
        .map_err(|e| format!("open ssh channel: {e}"))?;
    channel
        .request_pty(false, "xterm-256color", 80, 24, 0, 0, &[])
        .await
        .map_err(|e| format!("request pty: {e}"))?;
    channel
        .request_shell(true)
        .await
        .map_err(|e| format!("request shell: {e}"))?;

    let (tx, mut rx) = mpsc::unbounded_channel::<Vec<u8>>();

    // Register the session BEFORE spawning the loops so `ssh_write` can find it
    // the instant we return Ok.
    {
        let mut guard = state
            .sessions
            .lock()
            .map_err(|_| String::from("ssh session state is unavailable"))?;
        guard.insert(session_id.clone(), SshHandle { stdin: tx });
    }

    // Single task owns the channel: it reads channel messages (→ frontend) and
    // drains the stdin mpsc (→ channel.data). Keeping both on one task means the
    // non-`Sync` channel never crosses tasks. `handle` is moved in so the SSH
    // session stays alive for the channel's lifetime.
    let sessions_for_loop = Arc::clone(&state.sessions);
    tauri::async_runtime::spawn(async move {
        // Keep the connection handle alive for as long as the channel runs.
        let _handle = handle;
        loop {
            tokio::select! {
                // Frontend → server.
                msg = rx.recv() => {
                    match msg {
                        Some(bytes) => {
                            if channel.data(&bytes[..]).await.is_err() {
                                break;
                            }
                        }
                        // Sender dropped (ssh_close) — close the channel.
                        None => {
                            let _ = channel.eof().await;
                            break;
                        }
                    }
                }
                // Server → frontend.
                event = channel.wait() => {
                    match event {
                        Some(ChannelMsg::Data { data }) => {
                            let text = String::from_utf8_lossy(&data).to_string();
                            let _ = app.emit(
                                SSH_OUTPUT_EVENT,
                                SshOutputDto {
                                    session_id: session_id.clone(),
                                    data: text,
                                },
                            );
                        }
                        // stderr arrives as extended data — stream it too.
                        Some(ChannelMsg::ExtendedData { data, .. }) => {
                            let text = String::from_utf8_lossy(&data).to_string();
                            let _ = app.emit(
                                SSH_OUTPUT_EVENT,
                                SshOutputDto {
                                    session_id: session_id.clone(),
                                    data: text,
                                },
                            );
                        }
                        Some(ChannelMsg::Eof)
                        | Some(ChannelMsg::ExitStatus { .. })
                        | Some(ChannelMsg::Close)
                        | None => break,
                        // Other control messages (window adjust, etc.) — ignore.
                        Some(_) => {}
                    }
                }
            }
        }

        // Read loop ended (server closed, EOF, or stdin dropped): drop the
        // session and tell the frontend.
        if let Ok(mut guard) = sessions_for_loop.lock() {
            guard.remove(&session_id);
        }
        let _ = app.emit(SSH_EXIT_EVENT, SshExitDto { session_id });
    });

    Ok(())
}

/// Write bytes to the open shell's stdin.
#[tauri::command]
pub async fn ssh_write(
    state: State<'_, SshSessions>,
    session_id: String,
    data: String,
) -> Result<(), String> {
    let stdin = {
        let guard = state
            .sessions
            .lock()
            .map_err(|_| String::from("ssh session state is unavailable"))?;
        guard
            .get(session_id.trim())
            .map(|s| s.stdin.clone())
            .ok_or_else(|| String::from("ssh session not found"))?
    };
    stdin
        .send(data.into_bytes())
        .map_err(|_| String::from("ssh session is closed"))
}

/// Close the session: remove it and drop the stdin sender (which ends the
/// writer side and closes the channel).
#[tauri::command]
pub async fn ssh_close(state: State<'_, SshSessions>, session_id: String) -> Result<(), String> {
    let mut guard = state
        .sessions
        .lock()
        .map_err(|_| String::from("ssh session state is unavailable"))?;
    guard.remove(session_id.trim());
    // Dropping the removed `SshHandle` drops its `stdin` sender; the read/write
    // task sees `rx.recv() == None`, closes the channel, and emits `ssh://exit`.
    Ok(())
}
