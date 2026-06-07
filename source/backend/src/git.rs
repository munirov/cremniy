use std::path::Path;
use std::process::Command;

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
}
