#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
  tauri::Builder::default()
    .plugin(tauri_plugin_dialog::init())
    .invoke_handler(tauri::generate_handler![
      pick_folder,
      get_app_config_dir,
      read_text_file,
      write_app_config,
      read_app_preferences,
      save_app_preferences,
    ])
    .run(tauri::generate_context!())
    .expect("error while running tauri application");
}

use std::io::ErrorKind;
use std::path::{Component, Path, PathBuf};

use tauri::{AppHandle, Manager};
use tauri_plugin_dialog::DialogExt;

const PREFERENCES_RELATIVE_PATH: &str = "preferences.json";

#[tauri::command]
fn pick_folder(app: AppHandle) -> Option<String> {
  app
    .dialog()
    .file()
    .set_title("Open project folder")
    .blocking_pick_folder()
    .and_then(|fp| fp.into_path().ok())
    .map(|pb| pb.to_string_lossy().into_owned())
}

#[tauri::command]
fn get_app_config_dir(app: AppHandle) -> Result<String, String> {
  app
    .path()
    .app_config_dir()
    .map(|p| p.to_string_lossy().into_owned())
    .map_err(|e| e.to_string())
}

#[tauri::command]
fn read_text_file(app: AppHandle, relative_path: String) -> Result<String, String> {
  let path = resolve_under_app_config(&app, &relative_path)?;
  std::fs::read_to_string(&path).map_err(|e| e.to_string())
}

#[tauri::command]
fn write_app_config(app: AppHandle, relative_path: String, contents: String) -> Result<(), String> {
  write_bytes_under_app_config(&app, &relative_path, contents.as_bytes())
}

#[tauri::command]
fn read_app_preferences(app: AppHandle) -> Result<String, String> {
  let path = resolve_under_app_config(&app, PREFERENCES_RELATIVE_PATH)?;
  match std::fs::read_to_string(&path) {
    Ok(s) => Ok(s),
    Err(e) if e.kind() == ErrorKind::NotFound => Ok("{}".to_string()),
    Err(e) => Err(e.to_string()),
  }
}

#[tauri::command]
fn save_app_preferences(app: AppHandle, json: String) -> Result<(), String> {
  serde_json::from_str::<serde_json::Value>(&json).map_err(|e| format!("preferences JSON: {e}"))?;
  write_bytes_under_app_config(&app, PREFERENCES_RELATIVE_PATH, json.as_bytes())
}

fn write_bytes_under_app_config(app: &AppHandle, relative_path: &str, contents: &[u8]) -> Result<(), String> {
  let path = resolve_under_app_config(app, relative_path)?;
  if let Some(dir) = path.parent() {
    std::fs::create_dir_all(dir).map_err(|e| e.to_string())?;
  }
  std::fs::write(&path, contents).map_err(|e| e.to_string())
}

fn resolve_under_app_config(app: &AppHandle, relative_path: &str) -> Result<PathBuf, String> {
  let trimmed = relative_path.trim();
  if trimmed.is_empty() {
    return Err(String::from("relative_path must not be empty"));
  }
  let rel = Path::new(trimmed);
  if rel.is_absolute() {
    return Err(String::from("path must be relative"));
  }
  if rel.components().any(|c| matches!(c, Component::ParentDir)) {
    return Err(String::from("path must not contain parent directory segments"));
  }
  let base = app.path().app_config_dir().map_err(|e| e.to_string())?;
  let joined = base.join(rel);
  if !joined.starts_with(&base) {
    return Err(String::from("resolved path escapes application config directory"));
  }
  Ok(joined)
}
