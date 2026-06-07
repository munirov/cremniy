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

    Ok(GitStatus {
        is_repo: true,
        branch,
        files,
    })
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
