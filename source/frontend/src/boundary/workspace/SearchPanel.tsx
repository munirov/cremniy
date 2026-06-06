import { useCallback, useEffect, useState } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import {
  replaceInFile,
  searchWorkspace,
  type SearchMatch,
  type SearchResponse,
} from '@infrastructure/tauri/bridge';

import { useIdeSession } from './IdeSessionContext';
import { FileIcon } from './fileIcons';
import { SearchIcon } from './activityBarIcons';

import styles from './SearchPanel.module.css';

/** Trim leading whitespace from the preview and shift the highlight offsets. */
function renderPreview(m: SearchMatch) {
  const lead = m.preview.length - m.preview.trimStart().length;
  const text = m.preview.slice(lead);
  const start = Math.max(0, m.matchStart - lead);
  const end = Math.max(start, m.matchEnd - lead);
  return (
    <>
      {text.slice(0, start)}
      <mark className={styles.mark}>{text.slice(start, end)}</mark>
      {text.slice(end)}
    </>
  );
}

/**
 * Search view — content search across the workspace via the Rust grep command.
 * Its own panel (not the file tree): query + case/word/regex toggles +
 * include/exclude globs, live (debounced) results that open the file on click.
 */
export function SearchPanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const { openFileAtLine, reloadCleanOpenBuffers } = useIdeSession();
  const [query, setQuery] = useState('');
  const [replacement, setReplacement] = useState('');
  const [matchCase, setMatchCase] = useState(false);
  const [wholeWord, setWholeWord] = useState(false);
  const [useRegex, setUseRegex] = useState(false);
  const [showReplace, setShowReplace] = useState(false);
  const [showFilters, setShowFilters] = useState(false);
  const [includes, setIncludes] = useState('');
  const [excludes, setExcludes] = useState('');
  const [result, setResult] = useState<SearchResponse | null>(null);
  const [busy, setBusy] = useState(false);
  const [replacing, setReplacing] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const runSearch = useCallback(async () => {
    const root = workspaceRoot?.path;
    if (root == null || root === '' || query.trim() === '') {
      setResult(null);
      setError(null);
      return;
    }
    setBusy(true);
    setError(null);
    try {
      const res = await searchWorkspace(root, query, { matchCase, wholeWord, useRegex, includes, excludes });
      setResult(res);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setResult(null);
    } finally {
      setBusy(false);
    }
  }, [workspaceRoot, query, matchCase, wholeWord, useRegex, includes, excludes]);

  // Debounced live search whenever the query, toggles or filters change.
  useEffect(() => {
    const timer = setTimeout(() => void runSearch(), 280);
    return () => clearTimeout(timer);
  }, [runSearch]);

  const replaceAll = useCallback(async () => {
    const root = workspaceRoot?.path;
    if (root == null || root === '' || result == null || result.files.length === 0) {
      return;
    }
    setReplacing(true);
    setError(null);
    try {
      for (const file of result.files) {
        try {
          await replaceInFile(root, file.path, query, replacement, { matchCase, wholeWord, useRegex });
        } catch (e) {
          setError(e instanceof Error ? e.message : String(e));
        }
      }
      await reloadCleanOpenBuffers();
      await runSearch();
    } finally {
      setReplacing(false);
    }
  }, [workspaceRoot, result, query, replacement, matchCase, wholeWord, useRegex, runSearch, reloadCleanOpenBuffers]);

  const fileCount = result?.files.length ?? 0;

  return (
    <div className={styles.panel}>
      <div className={styles.searchArea}>
        <button
          type="button"
          className={styles.expandBtn}
          title="Toggle Replace"
          aria-label="Toggle Replace"
          aria-pressed={showReplace}
          onClick={() => setShowReplace((v) => !v)}
        >
          <svg
            width="12"
            height="12"
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
            aria-hidden
            style={{ transform: showReplace ? 'rotate(90deg)' : 'none', transition: 'transform 0.12s ease' }}
          >
            <path d="m9 18 6-6-6-6" />
          </svg>
        </button>
        <div className={styles.fields}>
          <div className={styles.field}>
            <input
              className={styles.fieldInput}
              type="text"
              placeholder="Search"
              aria-label="Search"
              value={query}
              autoFocus
              onChange={(e) => setQuery(e.target.value)}
            />
            <div className={styles.fieldBtns}>
              <button type="button" className={`${styles.miniBtn} ${matchCase ? styles.miniBtnOn : ''}`} title="Match case" aria-pressed={matchCase} onClick={() => setMatchCase((v) => !v)}>
                Aa
              </button>
              <button type="button" className={`${styles.miniBtn} ${wholeWord ? styles.miniBtnOn : ''}`} title="Match whole word" aria-pressed={wholeWord} onClick={() => setWholeWord((v) => !v)}>
                ab
              </button>
              <button type="button" className={`${styles.miniBtn} ${useRegex ? styles.miniBtnOn : ''}`} title="Use regular expression" aria-pressed={useRegex} onClick={() => setUseRegex((v) => !v)}>
                .*
              </button>
              <button type="button" className={`${styles.miniBtn} ${showFilters ? styles.miniBtnOn : ''}`} title="Toggle search details" aria-pressed={showFilters} onClick={() => setShowFilters((v) => !v)}>
                …
              </button>
            </div>
          </div>
          {showReplace ? (
            <div className={styles.field}>
              <input
                className={`${styles.fieldInput} ${styles.fieldInputReplace}`}
                type="text"
                placeholder="Replace"
                aria-label="Replace"
                value={replacement}
                onChange={(e) => setReplacement(e.target.value)}
              />
              <div className={styles.fieldBtns}>
                <button
                  type="button"
                  className={styles.miniBtn}
                  title="Replace all in results"
                  aria-label="Replace all"
                  disabled={replacing || (result?.files.length ?? 0) === 0}
                  onClick={() => void replaceAll()}
                >
                  <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" aria-hidden>
                    <path d="M14 4a2 2 0 0 1 2-2" />
                    <path d="M16 10a2 2 0 0 1-2-2" />
                    <path d="M20 2a2 2 0 0 1 2 2" />
                    <path d="M22 8a2 2 0 0 1-2 2" />
                    <path d="m3 7 3 3 3-3" />
                    <path d="M6 10V5a3 3 0 0 1 3-3h1" />
                    <rect width="8" height="8" x="2" y="14" rx="2" />
                  </svg>
                </button>
              </div>
            </div>
          ) : null}
        </div>
      </div>

      {showFilters ? (
        <div className={styles.filters}>
          <input
            className={styles.filterInput}
            type="text"
            placeholder="files to include (e.g. *.ts, src/**)"
            aria-label="Files to include"
            value={includes}
            onChange={(e) => setIncludes(e.target.value)}
          />
          <input
            className={styles.filterInput}
            type="text"
            placeholder="files to exclude"
            aria-label="Files to exclude"
            value={excludes}
            onChange={(e) => setExcludes(e.target.value)}
          />
        </div>
      ) : null}

      <div className={styles.results}>
        {busy ? (
          <p className={styles.note}>Searching…</p>
        ) : error != null ? (
          <p className={styles.errorNote}>{error}</p>
        ) : result == null ? (
          <div className={styles.empty}>
            <SearchIcon size={22} />
            <p>Search file contents across the workspace.</p>
          </div>
        ) : fileCount === 0 ? (
          <p className={styles.note}>No results.</p>
        ) : (
          <>
            <p className={styles.summary}>
              {result.totalMatches} result{result.totalMatches === 1 ? '' : 's'} in {fileCount} file
              {fileCount === 1 ? '' : 's'}
              {result.truncated ? ' (truncated)' : ''}
            </p>
            {result.files.map((file) => (
              <div key={file.path} className={styles.fileGroup}>
                <div className={styles.fileHeader} title={file.path}>
                  <FileIcon name={file.name} size={14} />
                  <span className={styles.fileName}>{file.name}</span>
                  <span className={styles.fileCount}>{file.matches.length}</span>
                </div>
                {file.matches.map((m, i) => (
                  <button
                    key={i}
                    type="button"
                    className={styles.matchRow}
                    title={`${file.path}:${m.line}`}
                    onClick={() => void openFileAtLine(file.path, m.line)}
                  >
                    <span className={styles.matchLine}>{m.line}</span>
                    <span className={styles.matchPreview}>{renderPreview(m)}</span>
                  </button>
                ))}
              </div>
            ))}
          </>
        )}
      </div>
    </div>
  );
}
