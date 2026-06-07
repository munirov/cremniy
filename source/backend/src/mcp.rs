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

/// Render a single window's actual content to a PNG via Win32 `PrintWindow` with
/// `PW_RENDERFULLCONTENT`. Unlike a monitor grab + crop, this asks the window to
/// paint itself into our DC, so it works even when the window is hidden,
/// minimized, occluded by other windows, or off-screen. WebView2/Chromium honor
/// `PW_RENDERFULLCONTENT`, so the web UI is captured too.
#[cfg(windows)]
fn capture_window_png(win: &tauri::WebviewWindow) -> Result<Vec<u8>, String> {
    use std::ffi::c_void;
    use windows::Win32::Foundation::{HWND, RECT};
    use windows::Win32::Graphics::Gdi::{
        CreateCompatibleBitmap, CreateCompatibleDC, DeleteDC, DeleteObject, GetDC, GetDIBits,
        ReleaseDC, SelectObject, BITMAPINFO, BITMAPINFOHEADER, BI_RGB, DIB_RGB_COLORS, HGDIOBJ,
    };
    // PrintWindow + its flags live under Storage::Xps in this windows version,
    // not WindowsAndMessaging.
    use windows::Win32::Storage::Xps::{PrintWindow, PRINT_WINDOW_FLAGS};
    use windows::Win32::UI::WindowsAndMessaging::GetWindowRect;

    /// PrintWindow flag: render the full content of the window (DWM/Chromium).
    const PW_RENDERFULLCONTENT: u32 = 0x0000_0002;

    let hwnd: HWND = win.hwnd().map_err(|e| e.to_string())?;

    unsafe {
        let mut rect = RECT::default();
        GetWindowRect(hwnd, &mut rect).map_err(|e| e.to_string())?;
        let w = rect.right - rect.left;
        let h = rect.bottom - rect.top;
        if w <= 0 || h <= 0 {
            return Err("window has zero size".to_string());
        }

        let hdc_screen = GetDC(Some(hwnd));
        if hdc_screen.is_invalid() {
            return Err("GetDC failed".to_string());
        }
        let hdc_mem = CreateCompatibleDC(Some(hdc_screen));
        if hdc_mem.is_invalid() {
            ReleaseDC(Some(hwnd), hdc_screen);
            return Err("CreateCompatibleDC failed".to_string());
        }
        let hbmp = CreateCompatibleBitmap(hdc_screen, w, h);
        if hbmp.is_invalid() {
            let _ = DeleteDC(hdc_mem);
            ReleaseDC(Some(hwnd), hdc_screen);
            return Err("CreateCompatibleBitmap failed".to_string());
        }
        let old = SelectObject(hdc_mem, HGDIOBJ(hbmp.0));

        // Macro-free cleanup helper used on every error path below.
        let cleanup = |old_obj| {
            SelectObject(hdc_mem, old_obj);
            let _ = DeleteObject(HGDIOBJ(hbmp.0));
            let _ = DeleteDC(hdc_mem);
            ReleaseDC(Some(hwnd), hdc_screen);
        };

        if !PrintWindow(hwnd, hdc_mem, PRINT_WINDOW_FLAGS(PW_RENDERFULLCONTENT)).as_bool() {
            cleanup(old);
            return Err("PrintWindow failed".to_string());
        }

        let mut bmi = BITMAPINFO::default();
        bmi.bmiHeader = BITMAPINFOHEADER {
            biSize: std::mem::size_of::<BITMAPINFOHEADER>() as u32,
            biWidth: w,
            // Negative height → top-down rows (origin at top-left).
            biHeight: -h,
            biPlanes: 1,
            biBitCount: 32,
            biCompression: BI_RGB.0,
            ..Default::default()
        };

        let mut buf = vec![0u8; (w as usize) * (h as usize) * 4];
        let got = GetDIBits(
            hdc_mem,
            hbmp,
            0,
            h as u32,
            Some(buf.as_mut_ptr() as *mut c_void),
            &mut bmi,
            DIB_RGB_COLORS,
        );
        if got == 0 {
            cleanup(old);
            return Err("GetDIBits failed".to_string());
        }

        cleanup(old);

        // GDI gives BGRA; PNG wants RGBA. Swap B/R and force opaque alpha
        // (PrintWindow leaves the alpha channel unreliable).
        for px in buf.chunks_exact_mut(4) {
            px.swap(0, 2);
            px[3] = 255;
        }

        let img = image::RgbaImage::from_raw(w as u32, h as u32, buf)
            .ok_or("bad image buffer")?;
        let mut bytes: Vec<u8> = Vec::new();
        image::DynamicImage::ImageRgba8(img)
            .write_to(&mut std::io::Cursor::new(&mut bytes), image::ImageFormat::Png)
            .map_err(|e| e.to_string())?;
        Ok(bytes)
    }
}

/// Capture each Tauri window's actual content (see `capture_window_png`). `label`,
/// if given, filters by Tauri window-label substring (e.g. "main", "terminal").
/// Minimized/hidden windows are captured too — that's the point of PrintWindow.
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
        let png = match capture_window_png(&win) {
            Ok(p) => p,
            Err(e) => {
                last_err = Some(e);
                continue;
            }
        };
        let title = win.title().unwrap_or_else(|_| lbl.clone());
        items.push(json!({ "type": "text", "text": format!("{lbl} — {title}") }));
        items.push(json!({ "type": "image", "data": base64_encode(&png), "mimeType": "image/png" }));
    }
    if items.is_empty() {
        return Err(last_err.unwrap_or_else(|| "no capturable app windows".to_string()));
    }
    Ok(items)
}

/// Standard base64 (with padding) — small enough to hand-roll, avoids a dep.
fn base64_encode(data: &[u8]) -> String {
    const T: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::with_capacity((data.len() + 2) / 3 * 4);
    for chunk in data.chunks(3) {
        let b0 = chunk[0] as u32;
        let b1 = *chunk.get(1).unwrap_or(&0) as u32;
        let b2 = *chunk.get(2).unwrap_or(&0) as u32;
        let n = (b0 << 16) | (b1 << 8) | b2;
        out.push(T[((n >> 18) & 63) as usize] as char);
        out.push(T[((n >> 12) & 63) as usize] as char);
        out.push(if chunk.len() > 1 { T[((n >> 6) & 63) as usize] as char } else { '=' });
        out.push(if chunk.len() > 2 { T[(n & 63) as usize] as char } else { '=' });
    }
    out
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
