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
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct GitStatus {
    is_repo: bool,
    branch: Option<String>,
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
                files: Vec::new(),
            })
        }
    };

    let text = String::from_utf8_lossy(&output.stdout);
    let (branch, files) = parse_porcelain(&text, &root);
    Ok(GitStatus {
        is_repo: true,
        branch,
        files,
    })
}

/// Parse `git status --porcelain=v1 --branch` output into a branch name + file
/// list. Pure (only `pretty_path` string work, no process / FS), so it's
/// unit-tested against the M / A / D / ?? / rename shapes the live tree can't
/// always exercise.
fn parse_porcelain(text: &str, root: &Path) -> (Option<String>, Vec<GitFileStatus>) {
    let mut branch: Option<String> = None;
    let mut files: Vec<GitFileStatus> = Vec::new();
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("## ") {
            branch = Some(parse_branch(rest));
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
        let rel = unquote(&rel);
        let untracked = x == "?" && y == "?";
        let staged = !untracked && x != " " && x != "?";
        let name = Path::new(&rel)
            .file_name()
            .map(|n| n.to_string_lossy().into_owned())
            .unwrap_or_else(|| rel.clone());
        let abs_path = crate::pretty_path(root.join(&rel))
            .to_string_lossy()
            .into_owned();
        files.push(GitFileStatus {
            path: rel,
            abs_path,
            name,
            index_status: x.to_string(),
            work_status: y.to_string(),
            staged,
            untracked,
        });
    }
    (branch, files)
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
        parse_porcelain(text, Path::new("/repo"))
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
}
