use std::io::Write;
use std::path::Path;
use std::process::{Command, Stdio};

use serde::Serialize;

use crate::canonical_workspace_root;

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GitFileStatus {
    /// Repo-relative path (for display).
    path: String,
    /// Absolute path (for opening in the editor).
    abs_path: String,
    name: String,
    /// Porcelain index (staged) code, e.g. "M", "A", " ".
    index_status: String,
    /// Porcelain work-tree (unstaged) code.
    work_status: String,
    staged: bool,
    untracked: bool,
    /// True for an untracked *directory* — git collapses these to a single
    /// `path/` entry. The UI renders a folder glyph and doesn't open it as a file.
    is_dir: bool,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GitStatus {
    is_repo: bool,
    branch: Option<String>,
    /// Commits ahead / behind the upstream (0 when no upstream).
    ahead: u32,
    behind: u32,
    files: Vec<GitFileStatus>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct RepoRef {
    /// Absolute repo root (used as the workspace_root arg to the other commands).
    path: String,
    name: String,
}

/// Working-tree status via `git status --porcelain`. Returns `is_repo: false`
/// (not an error) when the folder isn't a git repo or git isn't on PATH, so the
/// panel can show a friendly empty state instead of a red error.
#[tauri::command]
pub fn git_status(workspace_root: String) -> Result<GitStatus, String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let output = Command::new("git")
        .arg("-C")
        .arg(&root)
        .args([
            "-c",
            "core.quotepath=false",
            "status",
            "--porcelain=v1",
            "--branch",
        ])
        .output();

    let output = match output {
        Ok(o) if o.status.success() => o,
        // git missing, or not a repo (exits 128) → treat as "no repo".
        _ => {
            return Ok(GitStatus {
                is_repo: false,
                branch: None,
                ahead: 0,
                behind: 0,
                files: Vec::new(),
            })
        }
    };

    let text = String::from_utf8_lossy(&output.stdout);
    let (branch, ahead, behind, files) = parse_porcelain(&text, &root);
    Ok(GitStatus {
        is_repo: true,
        branch,
        ahead,
        behind,
        files,
    })
}

/// Run a git subcommand in the workspace; map a non-zero exit to its stderr so
/// the UI can surface a real message ("nothing to commit", auth errors, …).
fn run_git(root: &Path, args: &[&str]) -> Result<String, String> {
    let out = Command::new("git")
        .arg("-C")
        .arg(root)
        .args(args)
        .output()
        .map_err(|e| format!("git not available: {e}"))?;
    if out.status.success() {
        Ok(String::from_utf8_lossy(&out.stdout).into_owned())
    } else {
        let err = String::from_utf8_lossy(&out.stderr);
        Err(err.trim().to_string())
    }
}

/// `git init` — turn the workspace folder into a repository.
#[tauri::command]
pub fn git_init(workspace_root: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["init"]).map(|_| ())
}

/// Stage the given repo-relative paths (`git add -- <paths>`).
#[tauri::command]
pub fn git_stage(workspace_root: String, paths: Vec<String>) -> Result<(), String> {
    if paths.is_empty() {
        return Ok(());
    }
    let root = canonical_workspace_root(&workspace_root)?;
    let mut args: Vec<&str> = vec!["add", "--"];
    args.extend(paths.iter().map(String::as_str));
    run_git(&root, &args).map(|_| ())
}

/// Unstage the given repo-relative paths (`git reset -q HEAD -- <paths>`).
#[tauri::command]
pub fn git_unstage(workspace_root: String, paths: Vec<String>) -> Result<(), String> {
    if paths.is_empty() {
        return Ok(());
    }
    let root = canonical_workspace_root(&workspace_root)?;
    let mut args: Vec<&str> = vec!["reset", "-q", "HEAD", "--"];
    args.extend(paths.iter().map(String::as_str));
    run_git(&root, &args).map(|_| ())
}

/// Commit the staged changes with the user's message. The commit is authored by
/// the repo's own git identity — no extra trailers are added. `amend` rewrites
/// the last commit (keeping its message when a new one isn't given).
#[tauri::command]
pub fn git_commit(workspace_root: String, message: String, amend: bool) -> Result<(), String> {
    let msg = message.trim();
    let root = canonical_workspace_root(&workspace_root)?;
    if amend {
        return if msg.is_empty() {
            run_git(&root, &["commit", "--amend", "--no-edit"]).map(|_| ())
        } else {
            run_git(&root, &["commit", "--amend", "-m", msg]).map(|_| ())
        };
    }
    if msg.is_empty() {
        return Err(String::from("Commit message is empty"));
    }
    run_git(&root, &["commit", "-m", msg]).map(|_| ())
}

/// `git push` — publish local commits to the upstream.
#[tauri::command]
pub fn git_push(workspace_root: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["push"]).map(|_| ())
}

/// `git pull` — integrate upstream commits into the working tree.
#[tauri::command]
pub fn git_pull(workspace_root: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["pull"]).map(|_| ())
}

/// List local branch names.
#[tauri::command]
pub fn git_branches(workspace_root: String) -> Result<Vec<String>, String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let out = run_git(&root, &["branch", "--format=%(refname:short)"])?;
    Ok(out
        .lines()
        .map(|l| l.trim().to_string())
        .filter(|l| !l.is_empty())
        .collect())
}

/// Switch to an existing branch.
#[tauri::command]
pub fn git_checkout(workspace_root: String, branch: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["checkout", branch.trim()]).map(|_| ())
}

/// Create a new branch from HEAD and switch to it.
#[tauri::command]
pub fn git_create_branch(workspace_root: String, name: String) -> Result<(), String> {
    let n = name.trim();
    if n.is_empty() {
        return Err(String::from("Branch name is empty"));
    }
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["checkout", "-b", n]).map(|_| ())
}

/// Discard working-tree changes for the given repo-relative paths. Untracked
/// files are removed; tracked files are reverted to HEAD.
#[tauri::command]
pub fn git_discard(workspace_root: String, paths: Vec<String>, untracked: bool) -> Result<(), String> {
    if paths.is_empty() {
        return Ok(());
    }
    let root = canonical_workspace_root(&workspace_root)?;
    if untracked {
        // `git clean -f -- <paths>` removes untracked files.
        let mut args: Vec<&str> = vec!["clean", "-f", "--"];
        args.extend(paths.iter().map(String::as_str));
        run_git(&root, &args).map(|_| ())
    } else {
        let mut args: Vec<&str> = vec!["checkout", "HEAD", "--"];
        args.extend(paths.iter().map(String::as_str));
        run_git(&root, &args).map(|_| ())
    }
}

/// Discover git repositories under the workspace — the root itself plus nested
/// repos (e.g. microservices). Bounded-depth walk that skips heavy directories.
#[tauri::command]
pub fn git_repos(workspace_root: String) -> Result<Vec<RepoRef>, String> {
    use ignore::WalkBuilder;
    let root = canonical_workspace_root(&workspace_root)?;
    let mut repos: Vec<RepoRef> = Vec::new();
    let mut builder = WalkBuilder::new(&root);
    builder
        .max_depth(Some(6))
        .hidden(false)
        .git_ignore(false)
        .git_global(false)
        .require_git(false)
        .filter_entry(|e| {
            let n = e.file_name().to_string_lossy();
            n != ".git" && n != "node_modules" && n != "target" && n != "dist"
        });
    for dent in builder.build().flatten() {
        if !dent.file_type().map(|t| t.is_dir()).unwrap_or(false) {
            continue;
        }
        if dent.path().join(".git").exists() {
            let path = crate::pretty_path(dent.path().to_path_buf())
                .to_string_lossy()
                .into_owned();
            let name = dent
                .path()
                .file_name()
                .map(|n| n.to_string_lossy().into_owned())
                .unwrap_or_else(|| path.clone());
            repos.push(RepoRef { path, name });
        }
    }
    repos.sort_by(|a, b| a.name.to_lowercase().cmp(&b.name.to_lowercase()));
    Ok(repos)
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GitRemote {
    name: String,
    /// Fetch URL with any embedded credentials stripped — safe to display.
    url: String,
}

/// Clone a remote repository into `parent_dir`, returning the absolute path of
/// the new repo folder (its `origin` remote is configured by clone). Auth is
/// host-agnostic and "no special-casing": SSH URLs use the system keys; for
/// https, call `git_save_credentials` first (stored in the OS credential
/// manager) or rely on git's own credential helper.
#[tauri::command]
pub fn git_clone(
    repo_url: String,
    parent_dir: String,
    dir_name: Option<String>,
) -> Result<String, String> {
    let url = repo_url.trim();
    if url.is_empty() {
        return Err(String::from("Repository URL is empty"));
    }
    let parent = canonical_workspace_root(&parent_dir)?;
    let name = match dir_name {
        Some(n) if !n.trim().is_empty() => n.trim().to_string(),
        _ => repo_dir_name(url),
    };
    if name.is_empty() {
        return Err(String::from("Could not determine a folder name from the URL"));
    }
    let target = parent.join(&name);
    if target.exists() {
        return Err(format!("'{name}' already exists in that folder"));
    }
    // Run from the parent so clone creates <name> beneath it.
    let out = Command::new("git")
        .arg("-C")
        .arg(&parent)
        .args(["clone", url])
        .arg(&name)
        .output()
        .map_err(|e| format!("git not available: {e}"))?;
    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).trim().to_string());
    }
    Ok(crate::pretty_path(target).to_string_lossy().into_owned())
}

/// Store credentials for an http(s) remote in git's credential store (the OS
/// credential manager, via the configured helper) using the `git credential
/// approve` protocol. The token never touches the repo config or the remote URL.
/// SSH remotes don't apply (they authenticate with keys).
#[tauri::command]
pub fn git_save_credentials(url: String, username: String, token: String) -> Result<(), String> {
    let (protocol, host) = https_protocol_host(&url).ok_or_else(|| {
        String::from("Credentials only apply to http(s) remotes (SSH authenticates with keys).")
    })?;
    let user = username.trim();
    let pass = token.trim();
    if pass.is_empty() {
        return Err(String::from("Token / password is empty"));
    }
    let input = format!("protocol={protocol}\nhost={host}\nusername={user}\npassword={pass}\n\n");
    let mut child = Command::new("git")
        .args(["credential", "approve"])
        .stdin(Stdio::piped())
        .stdout(Stdio::null())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("git not available: {e}"))?;
    child
        .stdin
        .as_mut()
        .ok_or_else(|| String::from("failed to open git stdin"))?
        .write_all(input.as_bytes())
        .map_err(|e| format!("failed to write credentials: {e}"))?;
    let out = child.wait_with_output().map_err(|e| e.to_string())?;
    if out.status.success() {
        Ok(())
    } else {
        Err(String::from_utf8_lossy(&out.stderr).trim().to_string())
    }
}

/// List the repo's configured remotes (name + sanitized URL).
#[tauri::command]
pub fn git_remotes(workspace_root: String) -> Result<Vec<GitRemote>, String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let out = run_git(&root, &["remote", "-v"])?;
    let mut seen: Vec<GitRemote> = Vec::new();
    for line in out.lines() {
        // Each line: "<name>\t<url> (fetch|push)".
        let mut parts = line.split_whitespace();
        let name = match parts.next() {
            Some(n) => n.to_string(),
            None => continue,
        };
        let url = match parts.next() {
            Some(u) => sanitize_remote_url(u),
            None => continue,
        };
        if !seen.iter().any(|r| r.name == name) {
            seen.push(GitRemote { name, url });
        }
    }
    Ok(seen)
}

/// Add a remote, or update its URL if the name already exists.
#[tauri::command]
pub fn git_remote_add(workspace_root: String, name: String, url: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let n = name.trim();
    let u = url.trim();
    if n.is_empty() || u.is_empty() {
        return Err(String::from("Remote name and URL are required"));
    }
    if run_git(&root, &["remote", "get-url", n]).is_ok() {
        run_git(&root, &["remote", "set-url", n, u]).map(|_| ())
    } else {
        run_git(&root, &["remote", "add", n, u]).map(|_| ())
    }
}

/// Remove a remote by name.
#[tauri::command]
pub fn git_remote_remove(workspace_root: String, name: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["remote", "remove", name.trim()]).map(|_| ())
}

/// Push the given branch and set its upstream (`-u`) to the remote — the first
/// "publish" push for a branch that has no upstream yet.
#[tauri::command]
pub fn git_publish(workspace_root: String, remote: String, branch: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["push", "-u", remote.trim(), branch.trim()]).map(|_| ())
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GitBranchInfo {
    /// Current branch name, or None for a detached HEAD.
    current: Option<String>,
    local: Vec<String>,
    remote: Vec<String>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GitStashEntry {
    index: u32,
    message: String,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GitCommit {
    hash: String,
    short_hash: String,
    author: String,
    date: String,
    subject: String,
}

/// Merge `branch` into the current branch.
#[tauri::command]
pub fn git_merge(workspace_root: String, branch: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["merge", &branch]).map(|_| ())
}

/// Abort an in-progress merge, restoring the pre-merge state.
#[tauri::command]
pub fn git_merge_abort(workspace_root: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["merge", "--abort"]).map(|_| ())
}

/// Rebase the current branch onto `branch`.
#[tauri::command]
pub fn git_rebase(workspace_root: String, branch: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["rebase", &branch]).map(|_| ())
}

/// Abort an in-progress rebase, restoring the original branch.
#[tauri::command]
pub fn git_rebase_abort(workspace_root: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["rebase", "--abort"]).map(|_| ())
}

/// Continue an in-progress rebase after conflicts are resolved. `core.editor=true`
/// keeps git from opening an editor for the commit message (non-interactive).
#[tauri::command]
pub fn git_rebase_continue(workspace_root: String) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["-c", "core.editor=true", "rebase", "--continue"]).map(|_| ())
}

/// Delete a local branch. `force` uses `-D` (drop even if unmerged) instead of `-d`.
#[tauri::command]
pub fn git_branch_delete(workspace_root: String, name: String, force: bool) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let flag = if force { "-D" } else { "-d" };
    run_git(&root, &["branch", flag, &name]).map(|_| ())
}

/// Local + remote branches plus the current branch (None when detached).
#[tauri::command]
pub fn git_branch_info(workspace_root: String) -> Result<GitBranchInfo, String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let local = run_git(&root, &["branch", "--format=%(refname:short)"])?
        .lines()
        .map(|l| l.trim().to_string())
        .filter(|l| !l.is_empty())
        .collect();
    let remote = run_git(&root, &["branch", "-r", "--format=%(refname:short)"])?
        .lines()
        .map(|l| l.trim().to_string())
        .filter(|l| !l.is_empty() && !l.contains("HEAD"))
        .collect();
    // Err here means detached HEAD (or no commits yet) — not a real failure.
    let current = run_git(&root, &["symbolic-ref", "--quiet", "--short", "HEAD"])
        .ok()
        .map(|s| s.trim().to_string());
    Ok(GitBranchInfo {
        current,
        local,
        remote,
    })
}

/// Fetch from the remote and prune deleted remote-tracking branches.
#[tauri::command]
pub fn git_fetch(workspace_root: String, remote: Option<String>) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let mut args: Vec<&str> = vec!["fetch", "--prune"];
    if let Some(r) = remote.as_deref().map(str::trim).filter(|s| !s.is_empty()) {
        args.push(r);
    }
    run_git(&root, &args).map(|_| ())
}

/// List the stash entries (index + message).
#[tauri::command]
pub fn git_stash_list(workspace_root: String) -> Result<Vec<GitStashEntry>, String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let out = run_git(&root, &["stash", "list", "--format=%gd%x1f%gs"])?;
    Ok(parse_stash_list(&out))
}

/// Stash the working-tree changes, with an optional message.
#[tauri::command]
pub fn git_stash_push(workspace_root: String, message: Option<String>) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let mut args: Vec<&str> = vec!["stash", "push"];
    let msg = message.as_deref().map(str::trim).unwrap_or("");
    if !msg.is_empty() {
        args.push("-m");
        args.push(msg);
    }
    run_git(&root, &args).map(|_| ())
}

/// Apply a stash entry by index, keeping it in the stash list.
#[tauri::command]
pub fn git_stash_apply(workspace_root: String, index: u32) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["stash", "apply", &format!("stash@{{{index}}}")]).map(|_| ())
}

/// Apply a stash entry by index and drop it from the stash list.
#[tauri::command]
pub fn git_stash_pop(workspace_root: String, index: u32) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["stash", "pop", &format!("stash@{{{index}}}")]).map(|_| ())
}

/// Drop a stash entry by index without applying it.
#[tauri::command]
pub fn git_stash_drop(workspace_root: String, index: u32) -> Result<(), String> {
    let root = canonical_workspace_root(&workspace_root)?;
    run_git(&root, &["stash", "drop", &format!("stash@{{{index}}}")]).map(|_| ())
}

/// Recent commits (newest first), up to `limit` (clamped to at least 1). A repo
/// with no commits yet isn't an error here — it maps to an empty list.
#[tauri::command]
pub fn git_log(workspace_root: String, limit: u32) -> Result<Vec<GitCommit>, String> {
    let root = canonical_workspace_root(&workspace_root)?;
    let n = limit.max(1).to_string();
    match run_git(
        &root,
        &[
            "log",
            "-n",
            &n,
            "--pretty=format:%H%x1f%h%x1f%an%x1f%ad%x1f%s",
            "--date=short",
        ],
    ) {
        Ok(out) => Ok(parse_log(&out)),
        // Fresh repo with no commits: `git log` exits non-zero. Surface an empty
        // history instead of a red error.
        Err(e) if is_no_commits_error(&e) => Ok(Vec::new()),
        Err(e) => Err(e),
    }
}

/// True when a `git log` error just means the repo has no commits / no valid
/// revision yet (rather than a genuine failure).
fn is_no_commits_error(stderr: &str) -> bool {
    let s = stderr.to_lowercase();
    s.contains("does not have any commits")
        || s.contains("bad revision")
        || s.contains("unknown revision")
        || s.contains("ambiguous argument")
}

/// Parse `git log --pretty=format:%H%x1f%h%x1f%an%x1f%ad%x1f%s` output. Each line
/// is 5 unit-separator (\u{1f})-delimited fields; malformed lines are skipped.
/// Pure (no process / FS), so it's unit-tested.
fn parse_log(text: &str) -> Vec<GitCommit> {
    text.lines()
        .filter_map(|line| {
            let mut f = line.split('\u{1f}');
            let hash = f.next()?.to_string();
            let short_hash = f.next()?.to_string();
            let author = f.next()?.to_string();
            let date = f.next()?.to_string();
            let subject = f.next()?.to_string();
            // A well-formed line has exactly 5 fields; reject extras.
            if f.next().is_some() {
                return None;
            }
            Some(GitCommit {
                hash,
                short_hash,
                author,
                date,
                subject,
            })
        })
        .collect()
}

/// Parse `git stash list --format=%gd%x1f%gs` output. Each line is
/// "stash@{N}\u{1f}<message>"; the index N is parsed out of the left field.
/// Malformed lines are skipped. Pure, so it's unit-tested.
fn parse_stash_list(text: &str) -> Vec<GitStashEntry> {
    text.lines()
        .filter_map(|line| {
            let (left, message) = line.split_once('\u{1f}')?;
            let index = parse_stash_index(left)?;
            Some(GitStashEntry {
                index,
                message: message.to_string(),
            })
        })
        .collect()
}

/// Pull the integer N out of a stash ref like "stash@{2}".
fn parse_stash_index(s: &str) -> Option<u32> {
    let inner = s.trim().strip_prefix("stash@{")?.strip_suffix('}')?;
    inner.parse().ok()
}

/// Derive a folder name from a clone URL: the last path segment, minus `.git`.
/// Handles both normal URLs and scp-like `git@host:owner/repo.git`.
fn repo_dir_name(url: &str) -> String {
    let trimmed = url.trim().trim_end_matches('/');
    let after_colon = trimmed.rsplit(':').next().unwrap_or(trimmed);
    let last = after_colon.rsplit('/').next().unwrap_or(after_colon);
    last.strip_suffix(".git").unwrap_or(last).to_string()
}

/// Extract `protocol` + `host` (with port if present) from an http(s) URL for the
/// git credential protocol. None for SSH / scp-like URLs (they use keys).
fn https_protocol_host(url: &str) -> Option<(String, String)> {
    let u = url.trim();
    let (proto, rest) = if let Some(r) = u.strip_prefix("https://") {
        ("https", r)
    } else if let Some(r) = u.strip_prefix("http://") {
        ("http", r)
    } else {
        return None;
    };
    // Drop any user[:pass]@ prefix, then take up to the first '/'.
    let after_at = rest.rsplit_once('@').map(|(_, h)| h).unwrap_or(rest);
    let host = after_at.split('/').next().unwrap_or("");
    if host.is_empty() {
        None
    } else {
        Some((proto.to_string(), host.to_string()))
    }
}

/// Strip any embedded `user[:pass]@` from an http(s) URL so tokens never surface
/// in the UI. Non-http URLs (ssh) are returned unchanged.
fn sanitize_remote_url(url: &str) -> String {
    for proto in ["https://", "http://"] {
        if let Some(rest) = url.strip_prefix(proto) {
            if let Some((_creds, host_path)) = rest.split_once('@') {
                return format!("{proto}{host_path}");
            }
            return url.to_string();
        }
    }
    url.to_string()
}

/// Parse `git status --porcelain=v1 --branch` output into a branch name + file
/// list. Pure (only `pretty_path` string work, no process / FS), so it's
/// unit-tested against the M / A / D / ?? / rename shapes the live tree can't
/// always exercise.
fn parse_porcelain(text: &str, root: &Path) -> (Option<String>, u32, u32, Vec<GitFileStatus>) {
    let mut branch: Option<String> = None;
    let mut ahead = 0u32;
    let mut behind = 0u32;
    let mut files: Vec<GitFileStatus> = Vec::new();
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("## ") {
            branch = Some(parse_branch(rest));
            let (a, b) = parse_ahead_behind(rest);
            ahead = a;
            behind = b;
            continue;
        }
        if line.len() < 3 {
            continue;
        }
        let x = &line[0..1];
        let y = &line[1..2];
        let mut rel = line[3..].to_string();
        // Rename / copy entries read "old -> new"; keep the new path.
        if let Some(idx) = rel.find(" -> ") {
            rel = rel[idx + 4..].to_string();
        }
        let raw = unquote(&rel);
        // Untracked directories arrive as "path/" — strip the slash, flag as dir.
        let is_dir = raw.ends_with('/');
        let rel = raw.trim_end_matches('/').to_string();
        let untracked = x == "?" && y == "?";
        let staged = !untracked && x != " " && x != "?";
        let name = Path::new(&rel)
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_else(|| rel.clone());
        // Porcelain paths use '/'. Joining them onto a Windows root with
        // `root.join("a/b")` yields a mixed "C:\root\a/b" the editor can't open,
        // so build the absolute path segment-by-segment (native separator).
        let mut abs = root.to_path_buf();
        for seg in rel.split('/') {
            abs.push(seg);
        }
        let abs_path = crate::pretty_path(abs).to_string_lossy().into_owned();
        files.push(GitFileStatus {
            path: rel,
            abs_path,
            name,
            index_status: x.to_string(),
            work_status: y.to_string(),
            staged,
            untracked,
            is_dir,
        });
    }
    (branch, ahead, behind, files)
}

/// Pull the branch name out of a porcelain `## ` header line, e.g.
/// "main...origin/main [ahead 1]" → "main", or "No commits yet on main" → "main".
fn parse_branch(s: &str) -> String {
    if let Some(rest) = s.strip_prefix("No commits yet on ") {
        return rest.trim().to_string();
    }
    let head = s.split("...").next().unwrap_or(s);
    head.split(" [").next().unwrap_or(head).trim().to_string()
}

/// Read "ahead N" / "behind M" from a porcelain header's "[ahead 1, behind 2]"
/// suffix. Missing → 0.
fn parse_ahead_behind(s: &str) -> (u32, u32) {
    (count_after(s, "ahead "), count_after(s, "behind "))
}

fn count_after(s: &str, key: &str) -> u32 {
    match s.find(key) {
        Some(i) => s[i + key.len()..]
            .chars()
            .take_while(|c| c.is_ascii_digit())
            .collect::<String>()
            .parse()
            .unwrap_or(0),
        None => 0,
    }
}

/// Strip the surrounding quotes git adds around paths with unusual characters.
fn unquote(s: &str) -> String {
    let t = s.trim();
    if t.len() >= 2 && t.starts_with('"') && t.ends_with('"') {
        t[1..t.len() - 1].to_string()
    } else {
        t.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn parse(text: &str) -> (Option<String>, Vec<GitFileStatus>) {
        let (branch, _ahead, _behind, files) = parse_porcelain(text, Path::new("/repo"));
        (branch, files)
    }

    #[test]
    fn parses_branch_and_mixed_statuses() {
        // concat! keeps each literal exact — the leading space on unstaged
        // entries is significant (X = index, Y = work-tree).
        let text = concat!(
            "## main...origin/main [ahead 1]\n",
            "M  src/lib.rs\n",
            " M src/app.ts\n",
            "A  new.txt\n",
            " D gone.txt\n",
            "?? scratch.tmp\n",
        );
        let (branch, files) = parse(text);
        assert_eq!(branch.as_deref(), Some("main"));
        assert_eq!(files.len(), 5);

        let by = |p: &str| files.iter().find(|f| f.path == p).expect(p);
        assert!(by("src/lib.rs").staged && !by("src/lib.rs").untracked);
        assert_eq!(by("src/lib.rs").index_status, "M");
        assert!(!by("src/app.ts").staged); // unstaged modification
        assert_eq!(by("src/app.ts").work_status, "M");
        assert!(by("new.txt").staged);
        assert!(!by("gone.txt").staged); // unstaged delete
        assert!(by("scratch.tmp").untracked && !by("scratch.tmp").staged);
        assert_eq!(by("scratch.tmp").name, "scratch.tmp");
    }

    #[test]
    fn rename_keeps_the_new_path() {
        let (_b, files) = parse("## main\nR  old/a.ts -> new/b.ts\n");
        assert_eq!(files.len(), 1);
        assert_eq!(files[0].path, "new/b.ts");
        assert_eq!(files[0].name, "b.ts");
        assert!(files[0].staged);
    }

    #[test]
    fn no_commits_yet_branch() {
        let (b, _f) = parse("## No commits yet on trunk\n");
        assert_eq!(b.as_deref(), Some("trunk"));
    }

    #[test]
    fn unquotes_paths_with_spaces() {
        let (_b, files) = parse("## main\n?? \"weird name.txt\"\n");
        assert_eq!(files[0].path, "weird name.txt");
    }

    #[test]
    fn untracked_directory_is_flagged_and_slash_stripped() {
        let (_b, files) = parse("## main\n?? node_modules/\n?? docs/sub/\n");
        let nm = files.iter().find(|f| f.name == "node_modules").expect("node_modules");
        assert!(nm.is_dir && nm.untracked);
        assert_eq!(nm.path, "node_modules"); // trailing slash stripped → path not broken
        let sub = files.iter().find(|f| f.name == "sub").expect("sub");
        assert!(sub.is_dir);
        assert_eq!(sub.path, "docs/sub");
    }

    #[test]
    fn nested_file_abs_path_uses_native_separators() {
        use std::path::MAIN_SEPARATOR;
        let root_str = if cfg!(windows) { "C:\\repo" } else { "/repo" };
        let (_b, _a, _be, files) = parse_porcelain(" M src/a/b.txt\n", Path::new(root_str));
        let f = &files[0];
        assert_eq!(f.path, "src/a/b.txt"); // repo-relative stays '/' (git-friendly, display)
        assert_eq!(f.name, "b.txt");
        // The absolute path the editor opens must be all-native — no mixed seps.
        assert!(f.abs_path.ends_with(&format!("src{0}a{0}b.txt", MAIN_SEPARATOR)), "{}", f.abs_path);
        if cfg!(windows) {
            assert!(!f.abs_path.contains('/'), "mixed separators: {}", f.abs_path);
        }
    }

    #[test]
    fn parses_ahead_behind_counts() {
        let (_b, ahead, behind, _f) =
            parse_porcelain("## main...origin/main [ahead 2, behind 3]\n", Path::new("/repo"));
        assert_eq!(ahead, 2);
        assert_eq!(behind, 3);
        // No upstream bracket → zeros.
        let (_b2, a2, be2, _f2) = parse_porcelain("## main\n", Path::new("/repo"));
        assert_eq!((a2, be2), (0, 0));
    }

    #[test]
    fn repo_dir_name_from_various_urls() {
        assert_eq!(repo_dir_name("https://github.com/user/repo.git"), "repo");
        assert_eq!(repo_dir_name("https://gitverse.ru/user/My-Repo"), "My-Repo");
        assert_eq!(repo_dir_name("git@github.com:user/repo.git"), "repo");
        assert_eq!(repo_dir_name("https://host:8443/team/proj.git/"), "proj");
    }

    #[test]
    fn https_protocol_host_parses_http_skips_ssh() {
        assert_eq!(
            https_protocol_host("https://github.com/u/r.git"),
            Some(("https".into(), "github.com".into()))
        );
        assert_eq!(
            https_protocol_host("https://user:tok@gitverse.ru/u/r.git"),
            Some(("https".into(), "gitverse.ru".into()))
        );
        assert_eq!(
            https_protocol_host("https://host:8443/u/r.git"),
            Some(("https".into(), "host:8443".into()))
        );
        assert_eq!(https_protocol_host("git@github.com:u/r.git"), None);
    }

    #[test]
    fn sanitize_remote_url_strips_credentials() {
        assert_eq!(
            sanitize_remote_url("https://user:token@github.com/u/r.git"),
            "https://github.com/u/r.git"
        );
        assert_eq!(
            sanitize_remote_url("https://github.com/u/r.git"),
            "https://github.com/u/r.git"
        );
        assert_eq!(sanitize_remote_url("git@github.com:u/r.git"), "git@github.com:u/r.git");
    }

    #[test]
    fn parse_log_reads_fields_and_skips_malformed() {
        // \u{1f} is the field separator git emits for %x1f.
        let text = concat!(
            "abc123\u{1f}abc\u{1f}Alice\u{1f}2024-01-02\u{1f}Initial commit\n",
            "def456\u{1f}def\u{1f}Bob\u{1f}2024-03-04\u{1f}Fix: thing with: colons\n",
            "tooFewFields\u{1f}only\u{1f}three\n", // malformed → skipped
        );
        let commits = parse_log(text);
        assert_eq!(commits.len(), 2);
        assert_eq!(commits[0].hash, "abc123");
        assert_eq!(commits[0].short_hash, "abc");
        assert_eq!(commits[0].author, "Alice");
        assert_eq!(commits[0].date, "2024-01-02");
        assert_eq!(commits[0].subject, "Initial commit");
        // Subject is the last field; a literal colon in it is preserved.
        assert_eq!(commits[1].subject, "Fix: thing with: colons");
    }

    #[test]
    fn parse_log_empty_input_is_empty() {
        assert!(parse_log("").is_empty());
    }

    #[test]
    fn parse_stash_list_reads_index_and_message() {
        let text = concat!(
            "stash@{0}\u{1f}WIP on main: 1a2b3c subject\n",
            "stash@{2}\u{1f}On feature: hand-written message\n",
        );
        let entries = parse_stash_list(text);
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[0].index, 0);
        assert_eq!(entries[0].message, "WIP on main: 1a2b3c subject");
        assert_eq!(entries[1].index, 2);
        assert_eq!(entries[1].message, "On feature: hand-written message");
    }

    #[test]
    fn parse_stash_list_skips_malformed_lines() {
        let text = concat!(
            "stash@{0}\u{1f}good\n",
            "no-separator-here\n",          // no \u{1f} → skipped
            "stash@{notanumber}\u{1f}bad\n", // unparseable index → skipped
            "stash@{1}\u{1f}also good\n",
        );
        let entries = parse_stash_list(text);
        assert_eq!(entries.len(), 2);
        assert_eq!(entries[0].index, 0);
        assert_eq!(entries[1].index, 1);
        assert_eq!(entries[1].message, "also good");
    }
}
