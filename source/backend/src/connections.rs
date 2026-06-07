//! Persisted store of connection profiles for the "Connections" pack.
//!
//! Profiles (SSH / serial hosts) live in `connections.json` under the app
//! config dir — the same place preferences live (see `read_app_preferences` /
//! `save_app_preferences` in `lib.rs`). The list is read whole, mutated, and
//! written back; the frontend supplies a stable `id` (crypto.randomUUID) so
//! `conn_save` can upsert by id.

use std::io::ErrorKind;

use serde::{Deserialize, Serialize};
use tauri::AppHandle;

use crate::{read_string_under_app_config, write_bytes_under_app_config};

const CONNECTIONS_RELATIVE_PATH: &str = "connections.json";

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Connection {
    pub id: String,
    pub label: String,
    /// "ssh" | "serial".
    pub kind: String,
    pub group: Option<String>,
    pub tags: Vec<String>,
    pub ssh: Option<SshConn>,
    pub serial: Option<SerialConn>,
}

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SshConn {
    pub address: String,
    pub port: u16,
    pub username: String,
    // NOTE: the SSH password, if present, is stored in PLAINTEXT in
    // connections.json for now. This should move to the OS keychain later
    // (e.g. the same credential-manager path git credentials already use).
    pub password: Option<String>,
}

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SerialConn {
    pub port: String,
    pub baud: u32,
}

/// Read the saved connection profiles. Returns an empty list when the file
/// doesn't exist yet (first run).
#[tauri::command]
pub fn conn_list(app: AppHandle) -> Result<Vec<Connection>, String> {
    read_connections(&app)
}

/// Upsert a profile by `id` (replace if the id exists, else append) and write
/// the whole list back.
#[tauri::command]
pub fn conn_save(app: AppHandle, conn: Connection) -> Result<(), String> {
    let mut connections = read_connections(&app)?;
    match connections.iter_mut().find(|c| c.id == conn.id) {
        Some(existing) => *existing = conn,
        None => connections.push(conn),
    }
    write_connections(&app, &connections)
}

/// Delete a profile by `id`. Missing ids are a no-op.
#[tauri::command]
pub fn conn_delete(app: AppHandle, id: String) -> Result<(), String> {
    let mut connections = read_connections(&app)?;
    connections.retain(|c| c.id != id);
    write_connections(&app, &connections)
}

fn read_connections(app: &AppHandle) -> Result<Vec<Connection>, String> {
    match read_string_under_app_config(app, CONNECTIONS_RELATIVE_PATH) {
        Ok(text) => {
            serde_json::from_str(&text).map_err(|e| format!("connections.json: {e}"))
        }
        Err(e) if e.kind() == ErrorKind::NotFound => Ok(Vec::new()),
        Err(e) => Err(e.to_string()),
    }
}

fn write_connections(app: &AppHandle, connections: &[Connection]) -> Result<(), String> {
    let json = serde_json::to_vec_pretty(connections)
        .map_err(|e| format!("serialize connections: {e}"))?;
    write_bytes_under_app_config(app, CONNECTIONS_RELATIVE_PATH, &json)
}
