import { useCallback, useEffect, useState } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import {
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
  const { openFileAtLine } = useIdeSession();
  const [query, setQuery] = useState('');
  const [matchCase, setMatchCase] = useState(false);
  const [wholeWord, setWholeWord] = useState(false);
  const [useRegex, setUseRegex] = useState(false);
  const [showFilters, setShowFilters] = useState(false);
  const [includes, setIncludes] = useState('');
  const [excludes, setExcludes] = useState('');
  const [result, setResult] = useState<SearchResponse | null>(null);
  const [busy, setBusy] = useState(false);
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

  const fileCount = result?.files.length ?? 0;

  return (
    <div className={styles.panel}>
      <div className={styles.queryRow}>
        <input
          className={styles.queryInput}
          type="text"
          placeholder="Search"
          aria-label="Search"
          value={query}
          autoFocus
          onChange={(e) => setQuery(e.target.value)}
        />
        <button
          type="button"
          className={`${styles.toggle} ${matchCase ? styles.toggleOn : ''}`}
          title="Match case"
          aria-pressed={matchCase}
          onClick={() => setMatchCase((v) => !v)}
        >
          Aa
        </button>
        <button
          type="button"
          className={`${styles.toggle} ${wholeWord ? styles.toggleOn : ''}`}
          title="Match whole word"
          aria-pressed={wholeWord}
          onClick={() => setWholeWord((v) => !v)}
        >
          ab
        </button>
        <button
          type="button"
          className={`${styles.toggle} ${useRegex ? styles.toggleOn : ''}`}
          title="Use regular expression"
          aria-pressed={useRegex}
          onClick={() => setUseRegex((v) => !v)}
        >
          .*
        </button>
        <button
          type="button"
          className={`${styles.toggle} ${showFilters ? styles.toggleOn : ''}`}
          title="Toggle search details"
          aria-pressed={showFilters}
          onClick={() => setShowFilters((v) => !v)}
        >
          …
        </button>
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
