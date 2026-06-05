//! Pop-out pane windows.
//!
//! Frontend asks for a pane (file tree, editor, terminal, tool dock) to be
//! detached from the main window into its own native window. We open a new
//! WebviewWindow loading the same dev URL / bundled SPA at `/popout/<pane_id>`.
//! The frontend route renders only the requested pane full-screen and shares
//! state with the main window via Tauri events.

use tauri::{AppHandle, Emitter, Manager, WebviewUrl, WebviewWindowBuilder};

const POPOUT_LABEL_PREFIX: &str = "popout-";

fn label_for(pane_id: &str) -> String {
    format!("{POPOUT_LABEL_PREFIX}{pane_id}")
}

fn window_title(pane_id: &str) -> String {
    let pretty = match pane_id {
        "fileTree" => "Files",
        "editor" => "Editor",
        "terminal" => "Terminal",
        "toolDock" => "Tools",
        other => other,
    };
    format!("Cremniy — {pretty}")
}

/// Percent-encode a path so it survives as a URL query value. Encodes every
/// byte except the RFC 3986 unreserved set — covers backslashes, colons,
/// spaces and Cyrillic (e.g. "Рабочий стол") which all appear in Windows
/// paths.
fn percent_encode(s: &str) -> String {
    let mut out = String::with_capacity(s.len() * 2);
    for &b in s.as_bytes() {
        let unreserved = b.is_ascii_alphanumeric() || matches!(b, b'-' | b'_' | b'.' | b'~');
        if unreserved {
            out.push(b as char);
        } else {
            out.push('%');
            out.push_str(&format!("{b:02X}"));
        }
    }
    out
}

#[tauri::command]
pub fn popout_pane(app: AppHandle, pane_id: String, root: Option<String>) -> Result<String, String> {
    if pane_id.trim().is_empty() {
        return Err(String::from("pane_id must not be empty"));
    }
    let label = label_for(&pane_id);

    // Focus the existing window if it is already open.
    if let Some(window) = app.get_webview_window(&label) {
        let _ = window.set_focus();
        return Ok(label);
    }

    // Carry the workspace root into the popout URL so its WorkspaceProvider
    // resolves the same project the main window has open — otherwise the
    // detached pane renders against a null workspace (blank).
    let url_path = match root.as_deref().map(str::trim).filter(|r| !r.is_empty()) {
        Some(r) => format!("popout/{pane_id}?root={}", percent_encode(r)),
        None => format!("popout/{pane_id}"),
    };
    let window = WebviewWindowBuilder::new(&app, label.clone(), WebviewUrl::App(url_path.into()))
        .title(window_title(&pane_id))
        .inner_size(900.0, 700.0)
        .min_inner_size(360.0, 240.0)
        .resizable(true)
        // Match the main window: our own TitleBar is the only chrome, so no
        // native decorations (otherwise the popout shows two title bars).
        .decorations(false)
        .build()
        .map_err(|e| e.to_string())?;

    // When the popout closes, tell the main window so it can re-dock the pane.
    let app_for_event = app.clone();
    let pane_id_for_event = pane_id.clone();
    window.on_window_event(move |event| {
        if let tauri::WindowEvent::Destroyed = event {
            let _ = app_for_event.emit("pane:popout-closed", pane_id_for_event.clone());
        }
    });

    Ok(label)
}

#[tauri::command]
pub fn close_popout_pane(app: AppHandle, pane_id: String) -> Result<(), String> {
    let label = label_for(&pane_id);
    if let Some(window) = app.get_webview_window(&label) {
        window.close().map_err(|e| e.to_string())?;
    }
    Ok(())
}

#[tauri::command]
pub fn list_popout_panes(app: AppHandle) -> Vec<String> {
    app.webview_windows()
        .keys()
        .filter_map(|label| label.strip_prefix(POPOUT_LABEL_PREFIX).map(|s| s.to_string()))
        .collect()
}
