use std::env;
use std::ffi::{OsStr, OsString};
use std::io::ErrorKind;
use std::path::{Path, PathBuf};
use std::process::Command;

const OBJDUMP_TOOL_NAMES: [&str; 2] = ["objdump", "llvm-objdump"];
const DEFAULT_ARCH_HINT: &str = "i386:x86-64";
const OBJDUMP_ERROR_HINT: &str = "Install GNU binutils or LLVM tools, make sure objdump is available in PATH, or configure an absolute objdump executable path in Settings.";

#[derive(Clone, Copy, Debug, Eq, PartialEq, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
pub enum DisassemblySyntaxOption {
    Intel,
    Att,
}

#[derive(serde::Serialize)]
#[serde(rename_all = "camelCase")]
pub struct DisassemblyResultDto {
    executable: String,
    args: Vec<String>,
    cwd: String,
    file_path: String,
    stdout: String,
    stderr: String,
    status_code: Option<i32>,
    section_headers_stdout: String,
    section_headers_stderr: String,
    section_headers_status_code: Option<i32>,
}

#[tauri::command]
pub fn disassemble_workspace_file(
    workspace_root: String,
    file_path: String,
    objdump_path: Option<String>,
    arch_hint: Option<String>,
    syntax: Option<DisassemblySyntaxOption>,
    instruction_limit: Option<usize>,
) -> Result<DisassemblyResultDto, String> {
    let root_canon = canonical_workspace_directory(&workspace_root)?;
    let file_canon = canonical_workspace_file(&root_canon, &file_path)?;
    let executable_path = resolve_objdump_executable(Some(&root_canon), objdump_path)?;
    let executable = executable_path.to_string_lossy().into_owned();
    let effective_arch = normalize_arch_hint(arch_hint);
    let effective_syntax = normalize_syntax_option(syntax);
    let args = build_objdump_args(&file_canon, &effective_arch, effective_syntax);
    let header_args = vec![
        String::from("-h"),
        file_canon.to_string_lossy().into_owned(),
    ];

    let header_output = run_objdump(&executable_path, &header_args, &root_canon)?;
    let disassembly_output = run_objdump(&executable_path, &args, &root_canon)?;
    let disassembly_stdout = String::from_utf8_lossy(&disassembly_output.stdout).into_owned();
    let disassembly_truncated = exceeds_instruction_limit(&disassembly_stdout, instruction_limit);

    Ok(DisassemblyResultDto {
        executable,
        args,
        cwd: root_canon.to_string_lossy().into_owned(),
        file_path: file_canon.to_string_lossy().into_owned(),
        stdout: limit_disassembly_stdout(&disassembly_stdout, instruction_limit),
        stderr: append_instruction_limit_warning(
            String::from_utf8_lossy(&disassembly_output.stderr).into_owned(),
            instruction_limit.filter(|_| disassembly_truncated),
        ),
        status_code: disassembly_output.status.code(),
        section_headers_stdout: String::from_utf8_lossy(&header_output.stdout).into_owned(),
        section_headers_stderr: String::from_utf8_lossy(&header_output.stderr).into_owned(),
        section_headers_status_code: header_output.status.code(),
    })
}

#[tauri::command]
pub fn test_objdump_tool(
    workspace_root: Option<String>,
    objdump_path: Option<String>,
) -> Result<String, String> {
    let workspace_canon = workspace_root
        .as_deref()
        .map(str::trim)
        .filter(|value| !value.is_empty())
        .map(canonical_workspace_directory)
        .transpose()?;
    let executable_path = resolve_objdump_executable(workspace_canon.as_deref(), objdump_path)?;
    let output = run_objdump(
        &executable_path,
        &[String::from("--version")],
        Path::new("."),
    )?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr).trim().to_string();
        let detail = if stderr.is_empty() {
            format!("objdump exited with status {:?}", output.status.code())
        } else {
            stderr
        };
        return Err(detail);
    }

    let version_line = String::from_utf8_lossy(&output.stdout)
        .lines()
        .next()
        .map(str::trim)
        .filter(|line| !line.is_empty())
        .unwrap_or("objdump version check passed")
        .to_string();

    Ok(format!(
        "objdump OK: {} ({})",
        executable_path.to_string_lossy(),
        version_line
    ))
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

fn canonical_workspace_file(root_canon: &Path, file_path: &str) -> Result<PathBuf, String> {
    let file = PathBuf::from(file_path.trim());
    if file.as_os_str().is_empty() {
        return Err(String::from("file_path must not be empty"));
    }
    let file_canon = file.canonicalize().map_err(|e| format!("file_path: {e}"))?;
    if !file_canon.starts_with(root_canon) {
        return Err(String::from("file_path is outside workspace"));
    }
    let meta = std::fs::metadata(&file_canon).map_err(|e| e.to_string())?;
    if !meta.is_file() {
        return Err(String::from("file_path is not a regular file"));
    }
    Ok(file_canon)
}

fn normalize_arch_hint(arch_hint: Option<String>) -> String {
    arch_hint
        .map(|value| value.trim().to_string())
        .filter(|value| !value.is_empty())
        .unwrap_or_else(|| String::from(DEFAULT_ARCH_HINT))
}

fn normalize_syntax_option(syntax: Option<DisassemblySyntaxOption>) -> DisassemblySyntaxOption {
    syntax.unwrap_or(DisassemblySyntaxOption::Intel)
}

fn build_objdump_args(
    file_path: &Path,
    arch_hint: &str,
    syntax: DisassemblySyntaxOption,
) -> Vec<String> {
    let file_arg = file_path.to_string_lossy().into_owned();
    let is_raw_binary = file_path
        .extension()
        .and_then(|ext| ext.to_str())
        .is_some_and(|ext| ext.eq_ignore_ascii_case("bin"));

    let mut args = if is_raw_binary {
        vec![
            String::from("-D"),
            String::from("-b"),
            String::from("binary"),
            String::from("-m"),
            arch_hint.to_string(),
        ]
    } else {
        vec![
            String::from("-d"),
            String::from("-m"),
            arch_hint.to_string(),
        ]
    };

    if is_x86_arch(arch_hint) {
        args.push(String::from("-M"));
        args.push(objdump_syntax_arg(syntax).to_string());
    }
    args.push(file_arg);
    args
}

fn objdump_syntax_arg(syntax: DisassemblySyntaxOption) -> &'static str {
    match syntax {
        DisassemblySyntaxOption::Intel => "intel",
        DisassemblySyntaxOption::Att => "att",
    }
}

fn is_x86_arch(arch_hint: &str) -> bool {
    let arch = arch_hint.to_ascii_lowercase();
    arch.contains("i386") || arch.contains("x86")
}

fn resolve_objdump_executable(
    workspace_root: Option<&Path>,
    objdump_path: Option<String>,
) -> Result<PathBuf, String> {
    if let Some(path) = objdump_path
        .as_deref()
        .map(str::trim)
        .filter(|path| !path.is_empty())
    {
        return validate_configured_objdump_path(path, workspace_root);
    }

    let path_env = env::var_os("PATH").unwrap_or_default();
    let path_ext = env::var("PATHEXT").unwrap_or_default();

    resolve_tool_on_path(&OBJDUMP_TOOL_NAMES, &path_env, &path_ext, workspace_root)
        .ok_or_else(|| format!("objdump was not found. {OBJDUMP_ERROR_HINT}"))
}

fn validate_configured_objdump_path(
    objdump_path: &str,
    workspace_root: Option<&Path>,
) -> Result<PathBuf, String> {
    let candidate = PathBuf::from(objdump_path);
    if !candidate.is_absolute() {
        return Err(String::from("objdump path must be absolute"));
    }
    let candidate_canon = candidate
        .canonicalize()
        .map_err(|e| format!("objdump path: {e}"))?;
    let meta = std::fs::metadata(&candidate_canon).map_err(|e| e.to_string())?;
    if !meta.is_file() {
        return Err(String::from("objdump path is not a regular file"));
    }
    if let Some(workspace_root) = workspace_root {
        let workspace_canon = workspace_root
            .canonicalize()
            .unwrap_or_else(|_| workspace_root.to_path_buf());
        if is_path_inside_workspace(&candidate_canon, &workspace_canon) {
            return Err(String::from(
                "objdump path must not be inside the active workspace",
            ));
        }
    }
    if !is_executable_file(&candidate_canon, &meta) {
        return Err(String::from("objdump path is not executable"));
    }
    Ok(candidate_canon)
}

fn resolve_tool_on_path(
    tool_names: &[&str],
    path_env: &OsStr,
    path_ext: &str,
    workspace_root: Option<&Path>,
) -> Option<PathBuf> {
    let workspace_canon =
        workspace_root.map(|root| root.canonicalize().unwrap_or_else(|_| root.to_path_buf()));

    for dir in env::split_paths(path_env) {
        if !dir.is_absolute() {
            continue;
        }
        if workspace_canon
            .as_deref()
            .is_some_and(|root| is_path_inside_workspace(&dir, root))
        {
            continue;
        }

        for tool_name in tool_names {
            for executable_name in executable_names(tool_name, path_ext) {
                let candidate = dir.join(executable_name);
                if !candidate.is_file() {
                    continue;
                }

                let candidate_canon = candidate.canonicalize().unwrap_or(candidate);
                if workspace_canon
                    .as_deref()
                    .is_some_and(|root| is_path_inside_workspace(&candidate_canon, root))
                {
                    continue;
                }

                return Some(candidate_canon);
            }
        }
    }

    None
}

#[cfg(unix)]
fn is_executable_file(path: &Path, meta: &std::fs::Metadata) -> bool {
    use std::os::unix::fs::PermissionsExt;

    path.is_file() && meta.permissions().mode() & 0o111 != 0
}

#[cfg(windows)]
fn is_executable_file(path: &Path, _meta: &std::fs::Metadata) -> bool {
    path.is_file()
        && path
            .extension()
            .and_then(|ext| ext.to_str())
            .is_some_and(|ext| {
                matches!(
                    ext.to_ascii_lowercase().as_str(),
                    "exe" | "cmd" | "bat" | "com"
                )
            })
}

#[cfg(not(any(unix, windows)))]
fn is_executable_file(path: &Path, _meta: &std::fs::Metadata) -> bool {
    path.is_file()
}

fn is_path_inside_workspace(path: &Path, workspace_root: &Path) -> bool {
    path.starts_with(workspace_root)
        || path
            .canonicalize()
            .is_ok_and(|path_canon| path_canon.starts_with(workspace_root))
}

fn executable_names(tool_name: &str, path_ext: &str) -> Vec<OsString> {
    let mut names = vec![OsString::from(tool_name)];
    if Path::new(tool_name).extension().is_some() {
        return names;
    }

    for extension in path_ext
        .split(';')
        .map(str::trim)
        .filter(|ext| !ext.is_empty())
    {
        let normalized_extension = if extension.starts_with('.') {
            extension.to_string()
        } else {
            format!(".{extension}")
        };
        names.push(OsString::from(format!("{tool_name}{normalized_extension}")));
    }

    names
}

fn run_objdump(
    executable: &Path,
    args: &[String],
    cwd: &Path,
) -> Result<std::process::Output, String> {
    let output = Command::new(executable)
        .args(args)
        .current_dir(cwd)
        .env("LANG", "C")
        .env("LC_ALL", "C")
        .output();

    match output {
        Ok(output) => Ok(output),
        Err(e) if e.kind() == ErrorKind::NotFound => {
            Err(format!("objdump was not found. {OBJDUMP_ERROR_HINT}"))
        }
        Err(e) => Err(format!(
            "failed to start objdump: {e}. {OBJDUMP_ERROR_HINT}"
        )),
    }
}

fn limit_disassembly_stdout(stdout: &str, instruction_limit: Option<usize>) -> String {
    let Some(limit) = instruction_limit.filter(|value| *value > 0) else {
        return stdout.to_string();
    };

    let mut instruction_count = 0usize;
    let mut lines = Vec::new();
    for line in stdout.lines() {
        if is_instruction_line(line) {
            instruction_count += 1;
            if instruction_count > limit {
                continue;
            }
        }
        lines.push(line);
    }

    if stdout.ends_with('\n') {
        format!("{}\n", lines.join("\n"))
    } else {
        lines.join("\n")
    }
}

fn append_instruction_limit_warning(stderr: String, instruction_limit: Option<usize>) -> String {
    let Some(limit) = instruction_limit.filter(|value| *value > 0) else {
        return stderr;
    };
    let warning = format!("Cremniy returned at most {limit} disassembled instruction row(s).");
    if stderr.trim().is_empty() {
        warning
    } else {
        format!("{}\n{}", stderr.trim_end(), warning)
    }
}

fn exceeds_instruction_limit(stdout: &str, instruction_limit: Option<usize>) -> bool {
    let Some(limit) = instruction_limit.filter(|value| *value > 0) else {
        return false;
    };
    stdout
        .lines()
        .filter(|line| is_instruction_line(line))
        .count()
        > limit
}

fn is_instruction_line(line: &str) -> bool {
    let trimmed = line.trim_start();
    let Some((address, _rest)) = trimmed.split_once(':') else {
        return false;
    };
    !address.is_empty() && address.chars().all(|ch| ch.is_ascii_hexdigit())
}

#[cfg(test)]
mod tests {
    use super::{
        build_objdump_args, canonical_workspace_directory, canonical_workspace_file,
        limit_disassembly_stdout, normalize_arch_hint, normalize_syntax_option,
        resolve_objdump_executable, resolve_tool_on_path, run_objdump, DisassemblySyntaxOption,
    };
    use std::path::{Path, PathBuf};
    use std::time::{SystemTime, UNIX_EPOCH};
    use std::{env, fs};

    #[test]
    fn raw_bin_args_match_objdump_binary_mode() {
        let args = build_objdump_args(
            Path::new("/w/fw.bin"),
            "i386:x86-64",
            DisassemblySyntaxOption::Intel,
        );

        assert_eq!(
            args,
            vec![
                "-D",
                "-b",
                "binary",
                "-m",
                "i386:x86-64",
                "-M",
                "intel",
                "/w/fw.bin"
            ]
        );
    }

    #[test]
    fn object_file_args_use_disassemble_with_arch_hint() {
        let args = build_objdump_args(
            Path::new("/w/app.exe"),
            "i386",
            DisassemblySyntaxOption::Intel,
        );

        assert_eq!(args, vec!["-d", "-m", "i386", "-M", "intel", "/w/app.exe"]);
    }

    #[test]
    fn x86_objdump_args_honor_att_syntax_preference() {
        let args = build_objdump_args(
            Path::new("/w/app.exe"),
            "i386",
            DisassemblySyntaxOption::Att,
        );

        assert_eq!(args, vec!["-d", "-m", "i386", "-M", "att", "/w/app.exe"]);
    }

    #[test]
    fn non_x86_objdump_args_do_not_emit_syntax_modifier() {
        let args = build_objdump_args(Path::new("/w/fw.bin"), "arm", DisassemblySyntaxOption::Att);

        assert_eq!(args, vec!["-D", "-b", "binary", "-m", "arm", "/w/fw.bin"]);
    }

    #[test]
    fn arch_hint_defaults_to_x86_64() {
        assert_eq!(normalize_arch_hint(None), "i386:x86-64");
        assert_eq!(normalize_arch_hint(Some(String::from("  "))), "i386:x86-64");
    }

    #[test]
    fn syntax_option_defaults_to_intel() {
        assert_eq!(
            normalize_syntax_option(None),
            DisassemblySyntaxOption::Intel
        );
        assert_eq!(
            normalize_syntax_option(Some(DisassemblySyntaxOption::Att)),
            DisassemblySyntaxOption::Att
        );
    }

    #[test]
    fn workspace_file_rejects_paths_outside_workspace() {
        let root = unique_temp_path("root");
        let outside = unique_temp_path("outside.bin");
        fs::create_dir_all(&root).expect("created workspace root");
        fs::write(&outside, []).expect("created outside file");

        let root_canon = canonical_workspace_directory(root.to_string_lossy().as_ref())
            .expect("resolved workspace root");
        let error =
            canonical_workspace_file(&root_canon, outside.to_string_lossy().as_ref()).unwrap_err();

        assert_eq!(error, "file_path is outside workspace");

        fs::remove_file(outside).ok();
        fs::remove_dir_all(root).ok();
    }

    #[test]
    fn workspace_file_rejects_workspace_directories() {
        let root = unique_temp_path("root");
        let nested = root.join("nested");
        fs::create_dir_all(&nested).expect("created nested directory");

        let root_canon = canonical_workspace_directory(root.to_string_lossy().as_ref())
            .expect("resolved workspace root");
        let error =
            canonical_workspace_file(&root_canon, nested.to_string_lossy().as_ref()).unwrap_err();

        assert_eq!(error, "file_path is not a regular file");

        fs::remove_dir_all(root).ok();
    }

    #[test]
    fn missing_objdump_error_is_actionable() {
        let root = unique_temp_path("root");
        fs::create_dir_all(&root).expect("created workspace root");
        let missing_executable = root.join("missing-objdump-executable");

        let error = run_objdump(&missing_executable, &[], &root).unwrap_err();

        assert!(error.starts_with("objdump was not found."));
        assert!(error.contains("Install GNU binutils or LLVM tools"));
        assert!(error.contains("configure an absolute objdump executable path in Settings"));

        fs::remove_dir_all(root).ok();
    }

    #[test]
    fn objdump_resolution_rejects_workspace_local_executable() {
        let root = unique_temp_path("root");
        let trusted = unique_temp_path("trusted");
        fs::create_dir_all(&root).expect("created workspace root");
        fs::create_dir_all(&trusted).expect("created trusted tool directory");
        fs::write(root.join("objdump"), []).expect("created workspace-local objdump");
        fs::write(trusted.join("objdump"), []).expect("created trusted objdump");
        let path_env = env::join_paths([root.as_path(), trusted.as_path()]).expect("joined PATH");

        let resolved =
            resolve_tool_on_path(&["objdump"], path_env.as_os_str(), "", Some(&root)).unwrap();

        assert!(resolved.starts_with(trusted.canonicalize().expect("resolved trusted dir")));

        fs::remove_dir_all(root).ok();
        fs::remove_dir_all(trusted).ok();
    }

    #[test]
    fn objdump_resolution_rejects_workspace_path_entries() {
        let root = unique_temp_path("root");
        let tools = root.join("tools");
        fs::create_dir_all(&tools).expect("created workspace tool directory");
        fs::write(tools.join("objdump"), []).expect("created workspace-local objdump");
        let path_env = env::join_paths([tools.as_path()]).expect("joined PATH");

        let resolved = resolve_tool_on_path(&["objdump"], path_env.as_os_str(), "", Some(&root));

        assert!(resolved.is_none());

        fs::remove_dir_all(root).ok();
    }

    #[test]
    fn objdump_resolution_ignores_relative_path_entries() {
        let root = unique_temp_path("root");
        fs::create_dir_all(&root).expect("created workspace root");
        let path_env = PathBuf::from(".").into_os_string();

        let resolved = resolve_tool_on_path(&["objdump"], path_env.as_os_str(), "", Some(&root));

        assert!(resolved.is_none());

        fs::remove_dir_all(root).ok();
    }

    #[test]
    fn configured_objdump_path_must_be_absolute() {
        let error =
            resolve_objdump_executable(None, Some(String::from("tools/objdump"))).unwrap_err();

        assert_eq!(error, "objdump path must be absolute");
    }

    #[test]
    fn configured_objdump_path_trims_and_canonicalizes() {
        let root = unique_temp_path("root");
        let tools = unique_temp_path("trusted-tools");
        fs::create_dir_all(&root).expect("created workspace root");
        fs::create_dir_all(&tools).expect("created trusted tool directory");
        let executable = tools.join(configured_objdump_test_name());
        write_executable_file(&executable);

        let configured_path = format!("  {}  ", executable.to_string_lossy());
        let resolved =
            resolve_objdump_executable(Some(&root), Some(configured_path)).expect("resolved tool");

        assert_eq!(
            resolved,
            executable.canonicalize().expect("canonicalized executable")
        );

        fs::remove_dir_all(root).ok();
        fs::remove_dir_all(tools).ok();
    }

    #[test]
    fn configured_objdump_path_rejects_workspace_local_file() {
        let root = unique_temp_path("root");
        fs::create_dir_all(&root).expect("created workspace root");
        let executable = root.join("objdump");
        fs::write(&executable, []).expect("created workspace-local objdump");

        let error = resolve_objdump_executable(
            Some(&root),
            Some(executable.to_string_lossy().into_owned()),
        )
        .unwrap_err();

        assert_eq!(
            error,
            "objdump path must not be inside the active workspace"
        );

        fs::remove_dir_all(root).ok();
    }

    #[cfg(windows)]
    #[test]
    fn configured_objdump_path_rejects_windows_txt_file() {
        let tools = unique_temp_path("trusted-tools");
        fs::create_dir_all(&tools).expect("created trusted tool directory");
        let executable = tools.join("objdump.txt");
        fs::write(&executable, []).expect("created text file");

        let error =
            resolve_objdump_executable(None, Some(executable.to_string_lossy().into_owned()))
                .unwrap_err();

        assert_eq!(error, "objdump path is not executable");

        fs::remove_dir_all(tools).ok();
    }

    #[cfg(windows)]
    #[test]
    fn configured_objdump_path_accepts_windows_executable_extensions() {
        for extension in ["exe", "cmd", "bat", "com"] {
            let tools = unique_temp_path(extension);
            fs::create_dir_all(&tools).expect("created trusted tool directory");
            let executable = tools.join(format!("objdump.{extension}"));
            fs::write(&executable, []).expect("created executable file");

            let resolved =
                resolve_objdump_executable(None, Some(executable.to_string_lossy().into_owned()))
                    .expect("accepted executable extension");

            assert_eq!(
                resolved,
                executable.canonicalize().expect("canonicalized executable")
            );

            fs::remove_dir_all(tools).ok();
        }
    }

    #[test]
    fn disassembly_stdout_honors_instruction_limit() {
        let stdout = "\
Disassembly of section .text:
    1000:\t90                   \tnop
    1001:\t90                   \tnop
    1002:\t90                   \tnop
";

        let limited = limit_disassembly_stdout(stdout, Some(2));

        assert!(limited.contains("1000:"));
        assert!(limited.contains("1001:"));
        assert!(!limited.contains("1002:"));
        assert!(limited.contains("Disassembly of section .text:"));
    }

    fn unique_temp_path(label: &str) -> PathBuf {
        let nonce = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("system clock should be after unix epoch")
            .as_nanos();
        env::temp_dir().join(format!(
            "cremniy-disassembly-test-{}-{}-{label}",
            std::process::id(),
            nonce
        ))
    }

    fn write_executable_file(path: &Path) {
        fs::write(path, []).expect("created executable file");
        set_executable_permissions(path);
    }

    #[cfg(windows)]
    fn configured_objdump_test_name() -> &'static str {
        "objdump.exe"
    }

    #[cfg(not(windows))]
    fn configured_objdump_test_name() -> &'static str {
        "objdump"
    }

    #[cfg(unix)]
    fn set_executable_permissions(path: &Path) {
        use std::os::unix::fs::PermissionsExt;

        let mut permissions = fs::metadata(path)
            .expect("read executable metadata")
            .permissions();
        permissions.set_mode(0o755);
        fs::set_permissions(path, permissions).expect("marked executable");
    }

    #[cfg(not(unix))]
    fn set_executable_permissions(_path: &Path) {}
}
