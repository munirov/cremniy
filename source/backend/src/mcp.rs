//! In-app MCP server — exposes the running IDE to an MCP client (e.g. an AI
//! agent) so it can drive the UI and read its state.
//!
//! Transport: a tiny HTTP server on 127.0.0.1 speaking MCP JSON-RPC over POST.
//! Add it to an MCP client as an "http" server pointing at
//! `http://127.0.0.1:41547/mcp`.
//!
//! UI control isn't reimplemented here: tool calls are bridged to the webview's
//! `window.cremniy` registry (see documentation/architecture/AGENT_CONTROL.md)
//! over a Tauri event round-trip — `agent://request` out, `agent_reply` back —
//! so every command the UI already registers is callable, and window
//! enumeration is read straight off the Tauri app handle.

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::mpsc::{channel, Sender};
use std::sync::{Mutex, OnceLock};
use std::time::Duration;

use serde::Serialize;
use serde_json::{json, Value};
use tauri::{AppHandle, Emitter, Manager};

/// Local port for the MCP HTTP server. Client config:
/// `{ "type": "http", "url": "http://127.0.0.1:41547/mcp" }`.
const MCP_PORT: u16 = 41547;
const PROTOCOL_VERSION: &str = "2024-11-05";
/// How long a bridged UI call waits for the webview to answer.
const BRIDGE_TIMEOUT: Duration = Duration::from_secs(15);

struct Bridge {
    app: AppHandle,
    /// id → reply channel for in-flight bridged requests.
    pending: Mutex<HashMap<u64, Sender<(bool, String)>>>,
    counter: AtomicU64,
}

static BRIDGE: OnceLock<Bridge> = OnceLock::new();

#[derive(Serialize, Clone)]
struct AgentRequest {
    id: u64,
    /// "commands" | "state" | "run".
    kind: String,
    /// Command name for `run`.
    name: String,
    args: Value,
}

/// Initialize the bridge and start the HTTP server. Safe to call once; later
/// calls are no-ops.
pub fn start(app: AppHandle) {
    if BRIDGE
        .set(Bridge {
            app,
            pending: Mutex::new(HashMap::new()),
            counter: AtomicU64::new(1),
        })
        .is_err()
    {
        return;
    }
    std::thread::spawn(serve);
}

/// Webview → backend reply for a bridged `agent://request`; resolves the pending
/// MCP call by id.
#[tauri::command]
pub fn agent_reply(id: u64, ok: bool, json: String) {
    if let Some(bridge) = BRIDGE.get() {
        if let Some(tx) = bridge.pending.lock().unwrap().remove(&id) {
            let _ = tx.send((ok, json));
        }
    }
}

/// Emit a request to the webview's `window.cremniy` bridge and block for its
/// reply.
fn bridge_request(kind: &str, name: &str, args: Value) -> Result<Value, String> {
    let bridge = BRIDGE.get().ok_or("MCP bridge not initialized")?;
    let id = bridge.counter.fetch_add(1, Ordering::Relaxed);
    let (tx, rx) = channel::<(bool, String)>();
    bridge.pending.lock().unwrap().insert(id, tx);
    let emitted = bridge.app.emit(
        "agent://request",
        AgentRequest {
            id,
            kind: kind.to_string(),
            name: name.to_string(),
            args,
        },
    );
    if let Err(e) = emitted {
        bridge.pending.lock().unwrap().remove(&id);
        return Err(e.to_string());
    }
    let reply = rx.recv_timeout(BRIDGE_TIMEOUT);
    bridge.pending.lock().unwrap().remove(&id);
    match reply {
        Ok((true, payload)) => serde_json::from_str(&payload).map_err(|e| e.to_string()),
        Ok((false, err)) => Err(err),
        Err(_) => Err("the app window did not respond (is it open and loaded?)".to_string()),
    }
}

/// Enumerate the app's open windows straight off the Tauri handle.
fn list_windows() -> Result<Value, String> {
    let bridge = BRIDGE.get().ok_or("MCP bridge not initialized")?;
    let mut wins: Vec<Value> = Vec::new();
    for (label, w) in bridge.app.webview_windows() {
        wins.push(json!({
            "label": label,
            "title": w.title().unwrap_or_default(),
            "visible": w.is_visible().unwrap_or(false),
            "focused": w.is_focused().unwrap_or(false),
        }));
    }
    Ok(Value::Array(wins))
}

fn serve() {
    let server = match tiny_http::Server::http(("127.0.0.1", MCP_PORT)) {
        Ok(s) => s,
        // Port busy (e.g. a second instance) → no server, but don't crash.
        Err(_) => return,
    };
    for request in server.incoming_requests() {
        handle(request);
    }
}

fn json_header() -> tiny_http::Header {
    tiny_http::Header::from_bytes(&b"Content-Type"[..], &b"application/json"[..])
        .expect("static header")
}

fn handle(mut request: tiny_http::Request) {
    use tiny_http::{Method, Response};
    // Tools ride on POST. We don't open a GET SSE stream (no server-initiated
    // messages), so GET is simply not allowed.
    if request.method() != &Method::Post {
        let _ = request.respond(Response::from_string("").with_status_code(405));
        return;
    }
    let mut body = String::new();
    if std::io::Read::read_to_string(request.as_reader(), &mut body).is_err() {
        let _ = request.respond(Response::from_string("").with_status_code(400));
        return;
    }
    match process(&body) {
        Some(resp) => {
            let _ = request.respond(Response::from_string(resp).with_header(json_header()));
        }
        // Notification (no id) → no JSON-RPC body.
        None => {
            let _ = request.respond(Response::from_string("").with_status_code(202));
        }
    }
}

/// Process one JSON-RPC request body. Returns the response JSON, or None for a
/// notification (no reply expected).
fn process(body: &str) -> Option<String> {
    let req: Value = serde_json::from_str(body).ok()?;
    let method = req.get("method").and_then(Value::as_str).unwrap_or("");
    let id = req.get("id").cloned()?; // notifications have no id → no response
    let result: Result<Value, (i64, String)> = match method {
        "initialize" => Ok(json!({
            "protocolVersion": PROTOCOL_VERSION,
            "capabilities": { "tools": {} },
            "serverInfo": { "name": "cremniy", "version": env!("CARGO_PKG_VERSION") },
        })),
        "ping" => Ok(json!({})),
        "tools/list" => Ok(json!({ "tools": tool_defs() })),
        "tools/call" => call_tool(req.get("params")),
        other => Err((-32601, format!("Method not found: {other}"))),
    };
    let envelope = match result {
        Ok(r) => json!({ "jsonrpc": "2.0", "id": id, "result": r }),
        Err((code, msg)) => {
            json!({ "jsonrpc": "2.0", "id": id, "error": { "code": code, "message": msg } })
        }
    };
    Some(envelope.to_string())
}

fn call_tool(params: Option<&Value>) -> Result<Value, (i64, String)> {
    let params = params.ok_or((-32602, "missing params".to_string()))?;
    let name = params.get("name").and_then(Value::as_str).unwrap_or("");
    let args = params.get("arguments").cloned().unwrap_or_else(|| json!({}));
    // Screenshot returns image content, not text — handle it before the
    // text-wrapped tools.
    if name == "screenshot" {
        return Ok(screenshot(args.get("label").and_then(Value::as_str)));
    }
    let outcome: Result<Value, String> = match name {
        "list_commands" => bridge_request("commands", "", Value::Null),
        "get_state" => bridge_request("state", "", Value::Null),
        "list_windows" => list_windows(),
        "run_command" => {
            let cmd = args.get("name").and_then(Value::as_str).unwrap_or("");
            if cmd.is_empty() {
                Err("run_command requires a string { name }".to_string())
            } else {
                bridge_request("run", cmd, args.get("args").cloned().unwrap_or_else(|| json!({})))
            }
        }
        other => Err(format!("unknown tool: {other}")),
    };
    Ok(match outcome {
        Ok(v) => tool_text(
            &serde_json::to_string_pretty(&v).unwrap_or_else(|_| v.to_string()),
            false,
        ),
        Err(e) => tool_text(&e, true),
    })
}

fn tool_text(text: &str, is_error: bool) -> Value {
    json!({ "content": [ { "type": "text", "text": text } ], "isError": is_error })
}

/// Capture each of the app's windows as a PNG (MCP image content). Our windows
/// are identified by an OS title starting with "Cremniy" (see panes.rs for the
/// pop-out titles). `label`, if given, further filters by title substring.
fn screenshot(label: Option<&str>) -> Value {
    match capture_windows(label) {
        Ok(items) if !items.is_empty() => json!({ "content": items, "isError": false }),
        Ok(_) => tool_text("no Cremniy windows found to capture", true),
        Err(e) => tool_text(&e, true),
    }
}

/// Ask a window's webview to render ITSELF to a PNG (its own DOM → image, via
/// html-to-image in agentRemote.ts). This is the window photographing itself,
/// NOT an OS screen grab — so it works even when the window is minimized /
/// hidden / occluded, with no restore and no flash.
fn self_capture(label: &str) -> Result<String, String> {
    let bridge = BRIDGE.get().ok_or("MCP bridge not initialized")?;
    let win = bridge
        .app
        .get_webview_window(label)
        .ok_or_else(|| format!("window '{label}' not found"))?;
    let id = bridge.counter.fetch_add(1, Ordering::Relaxed);
    let (tx, rx) = channel::<(bool, String)>();
    bridge.pending.lock().unwrap().insert(id, tx);
    let emitted = win.emit(
        "agent://request",
        AgentRequest {
            id,
            kind: "capture".to_string(),
            name: String::new(),
            args: Value::Null,
        },
    );
    if let Err(e) = emitted {
        bridge.pending.lock().unwrap().remove(&id);
        return Err(e.to_string());
    }
    let reply = rx.recv_timeout(Duration::from_secs(20));
    bridge.pending.lock().unwrap().remove(&id);
    match reply {
        Ok((true, payload)) => {
            let v: Value = serde_json::from_str(&payload).map_err(|e| e.to_string())?;
            v.get("png")
                .and_then(Value::as_str)
                .map(|s| s.to_string())
                .ok_or_else(|| "capture reply had no png".to_string())
        }
        Ok((false, err)) => Err(err),
        Err(_) => Err("self-capture timed out".to_string()),
    }
}

fn capture_windows(label: Option<&str>) -> Result<Vec<Value>, String> {
    let bridge = BRIDGE.get().ok_or("MCP bridge not initialized")?;
    let mut items: Vec<Value> = Vec::new();
    let mut last_err: Option<String> = None;
    for (lbl, win) in bridge.app.webview_windows() {
        if let Some(l) = label {
            if !lbl.contains(l) {
                continue;
            }
        }
        // The window renders itself (works minimized/hidden, no flash).
        let png_b64 = match self_capture(&lbl) {
            Ok(b64) => b64,
            Err(e) => {
                last_err = Some(e);
                continue;
            }
        };
        let title = win.title().unwrap_or_else(|_| lbl.clone());
        items.push(json!({ "type": "text", "text": format!("{lbl} — {title}") }));
        items.push(json!({ "type": "image", "data": png_b64, "mimeType": "image/png" }));
    }
    if items.is_empty() {
        return Err(last_err.unwrap_or_else(|| "no capturable app windows".to_string()));
    }
    Ok(items)
}

fn tool_defs() -> Value {
    json!([
        {
            "name": "list_commands",
            "description": "List the UI commands available in the running Cremniy app (the window.cremniy registry). Names are namespaced, e.g. session.openFile, welcome.gotoClone.",
            "inputSchema": { "type": "object", "properties": {} }
        },
        {
            "name": "run_command",
            "description": "Run a Cremniy UI command by name with optional args. Example: {\"name\":\"session.openFile\",\"args\":{\"path\":\"src/main.rs\"}}. Use list_commands to discover what's available on the current screen.",
            "inputSchema": {
                "type": "object",
                "properties": {
                    "name": { "type": "string", "description": "Command id from list_commands." },
                    "args": { "type": "object", "description": "Arguments object for the command." }
                },
                "required": ["name"]
            }
        },
        {
            "name": "get_state",
            "description": "Snapshot the app's on-screen state (route, page, open file, sidebar view, ...).",
            "inputSchema": { "type": "object", "properties": {} }
        },
        {
            "name": "list_windows",
            "description": "List the app's open windows — the main window plus any popped-out panes (file tree, editor, terminal, tool dock).",
            "inputSchema": { "type": "object", "properties": {} }
        },
        {
            "name": "screenshot",
            "description": "Capture a PNG of each app window (main + popped-out panes) so you can see the UI. Optional { label } filters by Tauri window-label substring, e.g. \"main\" or \"terminal\".",
            "inputSchema": {
                "type": "object",
                "properties": { "label": { "type": "string", "description": "Optional window-label substring filter (e.g. main, popout-terminal)." } }
            }
        }
    ])
}
