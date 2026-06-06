use std::path::Path;

use ignore::overrides::OverrideBuilder;
use ignore::WalkBuilder;
use regex::RegexBuilder;
use serde::Serialize;

use crate::{canonical_workspace_root, pretty_path};

const MAX_FILE_BYTES: u64 = 2 * 1024 * 1024;
const MAX_PREVIEW_CHARS: usize = 400;
const DEFAULT_CAP: usize = 2000;

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SearchMatch {
    line: u32,
    column: u32,
    preview: String,
    /// Char offsets of the match inside `preview` (for highlighting).
    match_start: u32,
    match_end: u32,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SearchFileResult {
    path: String,
    name: String,
    matches: Vec<SearchMatch>,
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SearchResponse {
    files: Vec<SearchFileResult>,
    total_matches: usize,
    truncated: bool,
}

/// Content search across the workspace — ripgrep's walker (honours .gitignore +
/// the include/exclude globs) with a regex built from the query and toggles.
#[tauri::command]
#[allow(clippy::too_many_arguments)]
pub fn search_workspace(
    workspace_root: String,
    query: String,
    match_case: bool,
    whole_word: bool,
    use_regex: bool,
    includes: String,
    excludes: String,
    max_results: usize,
) -> Result<SearchResponse, String> {
    if query.trim().is_empty() {
        return Ok(SearchResponse {
            files: Vec::new(),
            total_matches: 0,
            truncated: false,
        });
    }
    let root = canonical_workspace_root(&workspace_root)?;
    let cap = if max_results == 0 { DEFAULT_CAP } else { max_results };

    let mut pattern = if use_regex {
        query.clone()
    } else {
        regex::escape(&query)
    };
    if whole_word {
        pattern = format!(r"\b(?:{pattern})\b");
    }
    let re = RegexBuilder::new(&pattern)
        .case_insensitive(!match_case)
        .build()
        .map_err(|e| format!("invalid pattern: {e}"))?;

    let mut ov = OverrideBuilder::new(&root);
    for g in parse_globs(&includes) {
        ov.add(&g).map_err(|e| format!("include glob '{g}': {e}"))?;
    }
    for g in parse_globs(&excludes) {
        ov.add(&format!("!{g}"))
            .map_err(|e| format!("exclude glob '{g}': {e}"))?;
    }
    let overrides = ov.build().map_err(|e| format!("globs: {e}"))?;

    let mut builder = WalkBuilder::new(&root);
    builder
        .hidden(false)
        .git_ignore(true)
        .git_global(false)
        .git_exclude(true)
        .overrides(overrides);

    let mut files: Vec<SearchFileResult> = Vec::new();
    let mut total = 0usize;
    let mut truncated = false;

    for dent in builder.build() {
        let dent = match dent {
            Ok(d) => d,
            Err(_) => continue,
        };
        if !dent.file_type().map(|t| t.is_file()).unwrap_or(false) {
            continue;
        }
        let path = dent.path();
        if path.metadata().map(|m| m.len()).unwrap_or(0) > MAX_FILE_BYTES {
            continue;
        }
        let bytes = match std::fs::read(path) {
            Ok(b) => b,
            Err(_) => continue,
        };
        if bytes.iter().take(8192).any(|&b| b == 0) {
            continue; // binary
        }
        let text = match String::from_utf8(bytes) {
            Ok(t) => t,
            Err(_) => continue,
        };

        let mut matches: Vec<SearchMatch> = Vec::new();
        for (idx, line) in text.lines().enumerate() {
            for m in re.find_iter(line) {
                let match_start = line[..m.start()].chars().count() as u32;
                let match_end = line[..m.end()].chars().count() as u32;
                matches.push(SearchMatch {
                    line: (idx + 1) as u32,
                    column: match_start + 1,
                    preview: line.chars().take(MAX_PREVIEW_CHARS).collect(),
                    match_start,
                    match_end,
                });
                total += 1;
                if total >= cap {
                    truncated = true;
                    break;
                }
            }
            if truncated {
                break;
            }
        }
        if !matches.is_empty() {
            files.push(SearchFileResult {
                path: pretty_path(path.to_path_buf())
                    .to_string_lossy()
                    .into_owned(),
                name: file_name(path),
                matches,
            });
        }
        if truncated {
            break;
        }
    }

    Ok(SearchResponse {
        files,
        total_matches: total,
        truncated,
    })
}

fn file_name(path: &Path) -> String {
    path.file_name()
        .map(|n| n.to_string_lossy().into_owned())
        .unwrap_or_default()
}

fn parse_globs(s: &str) -> Vec<String> {
    s.split([',', '\n'])
        .map(str::trim)
        .filter(|p| !p.is_empty())
        .map(String::from)
        .collect()
}
