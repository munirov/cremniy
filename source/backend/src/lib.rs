#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(terminal::TerminalSessions::default())
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![
            pick_folder,
            pick_file,
            pick_save_file,
            read_user_file,
            read_user_file_under_workspace,
            read_workspace_file_bytes,
            write_workspace_file_bytes,
            write_user_file,
            list_directory,
            create_project_folder,
            create_empty_file_under_workspace,
            create_directory_under_workspace,
            rename_under_workspace,
            delete_under_workspace,
            get_app_config_dir,
            read_text_file,
            write_app_config,
            read_app_preferences,
            save_app_preferences,
            disassembly::disassemble_workspace_file,
            disassembly::test_objdump_tool,
            process::run_workspace_command,
            terminal::start_terminal_session,
            terminal::write_terminal_input,
            terminal::stop_terminal_session,
            terminal::interrupt_terminal_session,
            terminal::get_terminal_capabilities,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

mod disassembly;
mod process;
mod terminal;

use std::io::{ErrorKind, Read, Write};
use std::path::{Component, Path, PathBuf};

use tauri::{AppHandle, Manager};
use tauri_plugin_dialog::DialogExt;

const PREFERENCES_RELATIVE_PATH: &str = "preferences.json";

/// Maximum bytes returned by `read_workspace_file_bytes` (single-shot read for binary tooling).
const MAX_WORKSPACE_FILE_READ_BYTES: u64 = 64 * 1024 * 1024;
/// Maximum bytes accepted by `write_workspace_file_bytes`.
const MAX_WORKSPACE_FILE_WRITE_BYTES: usize = 64 * 1024 * 1024;

#[tauri::command]
fn pick_folder(app: AppHandle) -> Option<String> {
    app.dialog()
        .file()
        .set_title("Open project folder")
        .blocking_pick_folder()
        .and_then(|fp| fp.into_path().ok())
        .map(|pb| pb.to_string_lossy().into_owned())
}

#[tauri::command]
fn pick_file(app: AppHandle) -> Option<String> {
    app.dialog()
        .file()
        .set_title("Open file")
        .blocking_pick_file()
        .and_then(|fp| fp.into_path().ok())
        .map(|pb| pb.to_string_lossy().into_owned())
}

#[tauri::command]
fn pick_save_file(app: AppHandle, default_path: Option<String>) -> Option<String> {
    let mut builder = app.dialog().file().set_title("Save file as");
    if let Some(ref p) = default_path {
        if let Some(name) = Path::new(p).file_name().and_then(|s| s.to_str()) {
            builder = builder.set_file_name(name);
        }
    }
    builder
        .blocking_save_file()
        .and_then(|fp| fp.into_path().ok())
        .map(|pb| pb.to_string_lossy().into_owned())
}

#[tauri::command]
fn read_user_file(path: String) -> Result<String, String> {
    let p = PathBuf::from(path.trim());
    if p.as_os_str().is_empty() {
        return Err(String::from("path must not be empty"));
    }
    let meta = std::fs::metadata(&p).map_err(|e| e.to_string())?;
    if !meta.is_file() {
        return Err(String::from("path is not a regular file"));
    }
    std::fs::read_to_string(&p).map_err(|e| e.to_string())
}

#[tauri::command]
fn read_user_file_under_workspace(workspace_root: String, path: String) -> Result<String, String> {
    let root = PathBuf::from(workspace_root.trim());
    let file_path = PathBuf::from(path.trim());
    if root.as_os_str().is_empty() {
        return Err(String::from("workspace_root must not be empty"));
    }
    if file_path.as_os_str().is_empty() {
        return Err(String::from("path must not be empty"));
    }
    let root_canon = root
        .canonicalize()
        .map_err(|e| format!("workspace_root: {e}"))?;
    let path_canon = file_path.canonicalize().map_err(|e| format!("path: {e}"))?;
    if !path_canon.starts_with(&root_canon) {
        return Err(String::from("path is outside workspace"));
    }
    let meta = std::fs::metadata(&path_canon).map_err(|e| e.to_string())?;
    if !meta.is_file() {
        return Err(String::from("path is not a regular file"));
    }
    std::fs::read_to_string(&path_canon).map_err(|e| e.to_string())
}

#[tauri::command]
fn read_workspace_file_bytes(workspace_root: String, path: String) -> Result<Vec<u8>, String> {
    let root = PathBuf::from(workspace_root.trim());
    let file_path = PathBuf::from(path.trim());
    if root.as_os_str().is_empty() {
        return Err(String::from("workspace_root must not be empty"));
    }
    if file_path.as_os_str().is_empty() {
        return Err(String::from("path must not be empty"));
    }
    let root_canon = root
        .canonicalize()
        .map_err(|e| format!("workspace_root: {e}"))?;
    let path_canon = file_path.canonicalize().map_err(|e| format!("path: {e}"))?;
    if !path_canon.starts_with(&root_canon) {
        return Err(String::from("path is outside workspace"));
    }
    let meta = std::fs::metadata(&path_canon).map_err(|e| e.to_string())?;
    if !meta.is_file() {
        return Err(String::from("path is not a regular file"));
    }
    let file = std::fs::File::open(&path_canon).map_err(|e| e.to_string())?;
    let mut buf = Vec::new();
    file.take(MAX_WORKSPACE_FILE_READ_BYTES.saturating_add(1))
        .read_to_end(&mut buf)
        .map_err(|e| e.to_string())?;
    if buf.len() as u64 > MAX_WORKSPACE_FILE_READ_BYTES {
        return Err(format!(
            "file exceeds maximum read size ({MAX_WORKSPACE_FILE_READ_BYTES} bytes)"
        ));
    }
    Ok(buf)
}

#[tauri::command]
fn write_workspace_file_bytes(
    workspace_root: String,
    path: String,
    bytes: Vec<u8>,
) -> Result<(), String> {
    if bytes.len() > MAX_WORKSPACE_FILE_WRITE_BYTES {
        return Err(format!(
            "file exceeds maximum write size ({MAX_WORKSPACE_FILE_WRITE_BYTES} bytes)"
        ));
    }

    let root_canon = canonical_workspace_root(&workspace_root)?;
    let file_path = PathBuf::from(path.trim());
    if file_path.as_os_str().is_empty() {
        return Err(String::from("path must not be empty"));
    }
    let path_canon = file_path.canonicalize().map_err(|e| format!("path: {e}"))?;
    assert_path_starts_with_root(&root_canon, &path_canon)?;
    let meta = std::fs::metadata(&path_canon).map_err(|e| e.to_string())?;
    if !meta.is_file() {
        return Err(String::from("path is not a regular file"));
    }

    write_bytes_atomically(&path_canon, &bytes)
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
struct DirectoryEntryDto {
    name: String,
    path: String,
    is_directory: bool,
}

#[tauri::command]
fn list_directory(
    workspace_root: String,
    dir_path: String,
) -> Result<Vec<DirectoryEntryDto>, String> {
    let root = PathBuf::from(workspace_root.trim());
    let dir = PathBuf::from(dir_path.trim());
    if root.as_os_str().is_empty() {
        return Err(String::from("workspace_root must not be empty"));
    }
    if dir.as_os_str().is_empty() {
        return Err(String::from("dir_path must not be empty"));
    }
    let root_canon = root
        .canonicalize()
        .map_err(|e| format!("workspace_root: {e}"))?;
    let dir_canon = dir.canonicalize().map_err(|e| format!("dir_path: {e}"))?;
    if !dir_canon.starts_with(&root_canon) {
        return Err(String::from("directory is outside workspace"));
    }
    let meta = std::fs::metadata(&dir_canon).map_err(|e| e.to_string())?;
    if !meta.is_dir() {
        return Err(String::from("path is not a directory"));
    }
    let mut entries: Vec<DirectoryEntryDto> = Vec::new();
    for entry in std::fs::read_dir(&dir_canon).map_err(|e| e.to_string())? {
        let entry = entry.map_err(|e| e.to_string())?;
        let meta = entry.metadata().map_err(|e| e.to_string())?;
        let name = entry.file_name().to_string_lossy().into_owned();
        let path = entry.path().to_string_lossy().into_owned();
        entries.push(DirectoryEntryDto {
            name,
            path,
            is_directory: meta.is_dir(),
        });
    }
    entries.sort_by(|a, b| match (a.is_directory, b.is_directory) {
        (true, false) => std::cmp::Ordering::Less,
        (false, true) => std::cmp::Ordering::Greater,
        _ => a.name.to_lowercase().cmp(&b.name.to_lowercase()),
    });
    Ok(entries)
}

fn canonical_workspace_root(workspace_root: &str) -> Result<PathBuf, String> {
    let root = PathBuf::from(workspace_root.trim());
    if root.as_os_str().is_empty() {
        return Err(String::from("workspace_root must not be empty"));
    }
    root.canonicalize()
        .map_err(|e| format!("workspace_root: {e}"))
}

fn is_valid_project_folder_name(s: &str) -> bool {
    !s.is_empty()
        && s.chars()
            .all(|c| c.is_ascii_alphanumeric() || c == '_' || c == '-')
}

fn assert_path_starts_with_root(root_canon: &Path, path_canon: &Path) -> Result<(), String> {
    if !path_canon.starts_with(root_canon) {
        return Err(String::from("path is outside workspace"));
    }
    Ok(())
}

fn write_bytes_atomically(path: &Path, bytes: &[u8]) -> Result<(), String> {
    let parent = path
        .parent()
        .ok_or_else(|| String::from("path has no parent directory"))?;
    let original_metadata = std::fs::metadata(path).map_err(|e| e.to_string())?;
    let mut temp = tempfile::Builder::new()
        .prefix(".cremniy-write-")
        .suffix(".tmp")
        .tempfile_in(parent)
        .map_err(|e| e.to_string())?;
    preserve_temp_permissions(temp.as_file(), &original_metadata)?;
    temp.write_all(bytes).map_err(|e| e.to_string())?;
    temp.as_file().sync_all().map_err(|e| e.to_string())?;
    temp.persist(path)
        .map(|_| ())
        .map_err(|e| e.error.to_string())
}

#[cfg(unix)]
fn preserve_temp_permissions(
    temp: &std::fs::File,
    original_metadata: &std::fs::Metadata,
) -> Result<(), String> {
    use std::os::unix::fs::PermissionsExt;

    let mode = original_metadata.permissions().mode();
    temp.set_permissions(std::fs::Permissions::from_mode(mode))
        .map_err(|e| e.to_string())
}

#[cfg(not(unix))]
fn preserve_temp_permissions(
    _temp: &std::fs::File,
    _original_metadata: &std::fs::Metadata,
) -> Result<(), String> {
    Ok(())
}

#[tauri::command]
fn create_project_folder(parent_path: String, folder_name: String) -> Result<String, String> {
    let trimmed_name = folder_name.trim();
    if trimmed_name.is_empty() {
        return Err(String::from("folder_name must not be empty"));
    }
    if !is_valid_project_folder_name(trimmed_name) {
        return Err(String::from("invalid folder name"));
    }
    let parent = PathBuf::from(parent_path.trim());
    if parent.as_os_str().is_empty() {
        return Err(String::from("parent_path must not be empty"));
    }
    let parent_canon = parent
        .canonicalize()
        .map_err(|e| format!("parent_path: {e}"))?;
    let meta = std::fs::metadata(&parent_canon).map_err(|e| e.to_string())?;
    if !meta.is_dir() {
        return Err(String::from("parent_path is not a directory"));
    }
    let dest = parent_canon.join(trimmed_name);
    if dest.exists() {
        return Err(String::from("destination already exists"));
    }
    std::fs::create_dir(&dest).map_err(|e| e.to_string())?;
    Ok(dest.to_string_lossy().into_owned())
}

#[tauri::command]
fn create_empty_file_under_workspace(
    workspace_root: String,
    file_path: String,
) -> Result<(), String> {
    let root_canon = canonical_workspace_root(&workspace_root)?;
    let fp = PathBuf::from(file_path.trim());
    if fp.as_os_str().is_empty() {
        return Err(String::from("file_path must not be empty"));
    }
    let parent = fp
        .parent()
        .ok_or_else(|| String::from("file_path has no parent"))?;
    let parent_canon = parent
        .canonicalize()
        .map_err(|e| format!("parent directory: {e}"))?;
    assert_path_starts_with_root(&root_canon, &parent_canon)?;
    if fp.exists() {
        return Err(String::from("file already exists"));
    }
    if let Some(dir) = fp.parent() {
        if !dir.as_os_str().is_empty() {
            std::fs::create_dir_all(dir).map_err(|e| e.to_string())?;
        }
    }
    std::fs::write(&fp, []).map_err(|e| e.to_string())
}

#[tauri::command]
fn create_directory_under_workspace(
    workspace_root: String,
    dir_path: String,
) -> Result<(), String> {
    let root_canon = canonical_workspace_root(&workspace_root)?;
    let dp = PathBuf::from(dir_path.trim());
    if dp.as_os_str().is_empty() {
        return Err(String::from("dir_path must not be empty"));
    }
    let parent = dp
        .parent()
        .ok_or_else(|| String::from("dir_path has no parent"))?;
    let parent_canon = parent
        .canonicalize()
        .map_err(|e| format!("parent directory: {e}"))?;
    assert_path_starts_with_root(&root_canon, &parent_canon)?;
    if dp.exists() {
        return Err(String::from("directory already exists"));
    }
    std::fs::create_dir(&dp).map_err(|e| e.to_string())
}

#[tauri::command]
fn rename_under_workspace(
    workspace_root: String,
    from_path: String,
    to_path: String,
) -> Result<(), String> {
    let root_canon = canonical_workspace_root(&workspace_root)?;
    let from = PathBuf::from(from_path.trim());
    let to = PathBuf::from(to_path.trim());
    if from.as_os_str().is_empty() || to.as_os_str().is_empty() {
        return Err(String::from("paths must not be empty"));
    }
    let from_canon = from.canonicalize().map_err(|e| format!("from_path: {e}"))?;
    let to_parent = to
        .parent()
        .ok_or_else(|| String::from("to_path has no parent"))?;
    let to_parent_canon = to_parent
        .canonicalize()
        .map_err(|e| format!("to_path parent: {e}"))?;
    assert_path_starts_with_root(&root_canon, &from_canon)?;
    assert_path_starts_with_root(&root_canon, &to_parent_canon)?;
    if to.exists() {
        return Err(String::from("destination already exists"));
    }
    std::fs::rename(&from_canon, &to).map_err(|e| e.to_string())
}

#[tauri::command]
fn delete_under_workspace(workspace_root: String, path: String) -> Result<(), String> {
    let root_canon = canonical_workspace_root(&workspace_root)?;
    let p = PathBuf::from(path.trim());
    if p.as_os_str().is_empty() {
        return Err(String::from("path must not be empty"));
    }
    let path_canon = p.canonicalize().map_err(|e| format!("path: {e}"))?;
    assert_path_starts_with_root(&root_canon, &path_canon)?;
    if path_canon == root_canon {
        return Err(String::from("cannot delete workspace root"));
    }
    let meta = std::fs::metadata(&path_canon).map_err(|e| e.to_string())?;
    if meta.is_dir() {
        std::fs::remove_dir_all(&path_canon).map_err(|e| e.to_string())?;
    } else {
        std::fs::remove_file(&path_canon).map_err(|e| e.to_string())?;
    }
    Ok(())
}

#[tauri::command]
fn write_user_file(path: String, contents: String) -> Result<(), String> {
    let p = PathBuf::from(path.trim());
    if p.as_os_str().is_empty() {
        return Err(String::from("path must not be empty"));
    }
    if let Some(dir) = p.parent() {
        if !dir.as_os_str().is_empty() {
            std::fs::create_dir_all(dir).map_err(|e| e.to_string())?;
        }
    }
    std::fs::write(&p, contents.as_bytes()).map_err(|e| e.to_string())
}

#[tauri::command]
fn get_app_config_dir(app: AppHandle) -> Result<String, String> {
    app.path()
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
    serde_json::from_str::<serde_json::Value>(&json)
        .map_err(|e| format!("preferences JSON: {e}"))?;
    write_bytes_under_app_config(&app, PREFERENCES_RELATIVE_PATH, json.as_bytes())
}

fn write_bytes_under_app_config(
    app: &AppHandle,
    relative_path: &str,
    contents: &[u8],
) -> Result<(), String> {
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
        return Err(String::from(
            "path must not contain parent directory segments",
        ));
    }
    let base = app.path().app_config_dir().map_err(|e| e.to_string())?;
    let joined = base.join(rel);
    if !joined.starts_with(&base) {
        return Err(String::from(
            "resolved path escapes application config directory",
        ));
    }
    Ok(joined)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn write_workspace_file_bytes_replaces_existing_file() {
        let temp = tempfile::tempdir().expect("created temp workspace");
        let file_path = temp.path().join("sample.bin");
        std::fs::write(&file_path, [0x01, 0x02]).expect("wrote initial bytes");

        write_workspace_file_bytes(
            temp.path().to_string_lossy().into_owned(),
            file_path.to_string_lossy().into_owned(),
            vec![0xaa, 0xbb, 0xcc],
        )
        .expect("wrote replacement bytes");

        let bytes = std::fs::read(file_path).expect("read replaced bytes");
        assert_eq!(bytes, vec![0xaa, 0xbb, 0xcc]);
    }

    #[cfg(unix)]
    #[test]
    fn write_workspace_file_bytes_preserves_unix_permissions() {
        use std::os::unix::fs::PermissionsExt;

        let temp = tempfile::tempdir().expect("created temp workspace");
        let file_path = temp.path().join("sample.bin");
        std::fs::write(&file_path, [0x01, 0x02]).expect("wrote initial bytes");
        std::fs::set_permissions(&file_path, std::fs::Permissions::from_mode(0o640))
            .expect("set initial permissions");

        write_workspace_file_bytes(
            temp.path().to_string_lossy().into_owned(),
            file_path.to_string_lossy().into_owned(),
            vec![0xaa, 0xbb, 0xcc],
        )
        .expect("wrote replacement bytes");

        let mode = std::fs::metadata(&file_path)
            .expect("read replaced metadata")
            .permissions()
            .mode();
        assert_eq!(mode & 0o777, 0o640);
    }

    #[test]
    fn write_workspace_file_bytes_rejects_paths_outside_workspace() {
        let workspace = tempfile::tempdir().expect("created temp workspace");
        let outside = tempfile::tempdir().expect("created outside dir");
        let outside_file = outside.path().join("outside.bin");
        std::fs::write(&outside_file, [0x01]).expect("wrote outside file");

        let err = write_workspace_file_bytes(
            workspace.path().to_string_lossy().into_owned(),
            outside_file.to_string_lossy().into_owned(),
            vec![0x02],
        )
        .expect_err("outside write should fail");

        assert_eq!(err, "path is outside workspace");
        assert_eq!(
            std::fs::read(outside_file).expect("read outside file"),
            vec![0x01]
        );
    }

    #[test]
    fn write_workspace_file_bytes_rejects_directories() {
        let temp = tempfile::tempdir().expect("created temp workspace");

        let err = write_workspace_file_bytes(
            temp.path().to_string_lossy().into_owned(),
            temp.path().to_string_lossy().into_owned(),
            vec![0x01],
        )
        .expect_err("directory write should fail");

        assert_eq!(err, "path is not a regular file");
    }

    #[test]
    fn write_workspace_file_bytes_rejects_payloads_over_size_cap() {
        let err = write_workspace_file_bytes(
            String::new(),
            String::new(),
            vec![0; MAX_WORKSPACE_FILE_WRITE_BYTES + 1],
        )
        .expect_err("oversize write should fail before resolving paths");

        assert_eq!(
            err,
            format!("file exceeds maximum write size ({MAX_WORKSPACE_FILE_WRITE_BYTES} bytes)")
        );
    }

    #[test]
    fn write_workspace_file_bytes_rejects_missing_files() {
        let temp = tempfile::tempdir().expect("created temp workspace");
        let missing = temp.path().join("missing.bin");

        let err = write_workspace_file_bytes(
            temp.path().to_string_lossy().into_owned(),
            missing.to_string_lossy().into_owned(),
            vec![0x01],
        )
        .expect_err("missing write target should fail");

        assert!(err.starts_with("path:"));
    }
}
