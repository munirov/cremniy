import { useEffect, useLayoutEffect, useMemo, useRef, useState, type UIEvent } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import { filterStrings, type ExtractedString } from '@domain/strings/stringExtraction';
import { extractWorkspaceFileStrings } from '@infrastructure/tauri/bridge';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './BinaryToolPanel.module.css';

const MIN_LENGTHS = [4, 5, 6, 8, 10, 16];
// Cap the result count, not the file size — the Rust side streams the file and
// stops here, so a huge binary lists its first strings without an inline read.
const STRING_LIMIT = 5000;
const ROW_PX = 21;
const ROW_BUFFER = 6;

export function StringsToolPanel() {
  const { activeFilePath, fileContentRevision, openPanel } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';

  const [strings, setStrings] = useState<ExtractedString[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [query, setQuery] = useState('');
  const [minLength, setMinLength] = useState(4);

  const scrollRef = useRef<HTMLDivElement>(null);
  const [scrollTop, setScrollTop] = useState(0);
  const [viewportHeight, setViewportHeight] = useState(0);

  useEffect(() => {
    if (activeFilePath == null || activeFilePath === '' || workspacePath === '') {
      setStrings(null);
      setError(null);
      return;
    }
    let cancelled = false;
    setLoading(true);
    setError(null);
    void extractWorkspaceFileStrings(workspacePath, activeFilePath, minLength, STRING_LIMIT)
      .then((extracted) => {
        if (cancelled) return;
        setStrings(extracted);
      })
      .catch((e: unknown) => {
        if (cancelled) return;
        setError(e instanceof Error ? e.message : String(e));
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, [activeFilePath, minLength, workspacePath, fileContentRevision]);

  const filtered = useMemo(() => (strings != null ? filterStrings(strings, query) : []), [query, strings]);

  // Virtualize the list: render only the visible window of rows so a 5000-string
  // result stays a few dozen DOM nodes, not thousands.
  useLayoutEffect(() => {
    const el = scrollRef.current;
    if (el == null) return;
    const ro = new ResizeObserver(() => setViewportHeight(el.clientHeight));
    ro.observe(el);
    setViewportHeight(el.clientHeight);
    return () => ro.disconnect();
  }, [activeFilePath]);

  useEffect(() => {
    setScrollTop(0);
    if (scrollRef.current != null) scrollRef.current.scrollTop = 0;
  }, [activeFilePath, query, minLength]);

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a file to extract ASCII strings.</p>
      </section>
    );
  }

  const firstVisibleRow = Math.max(0, Math.floor(scrollTop / ROW_PX) - ROW_BUFFER);
  const visibleRowCount = Math.ceil((viewportHeight || ROW_PX) / ROW_PX) + ROW_BUFFER * 2;
  const lastVisibleRow = Math.min(filtered.length, firstVisibleRow + visibleRowCount);
  const visibleStrings = filtered.slice(firstVisibleRow, lastVisibleRow);

  return (
    <section className={styles.root} aria-label="Strings">
      <p className={styles.subtitle} title={activeFilePath}>
        {fileNameFromPath(activeFilePath)}
      </p>
      <div className={styles.toolbar}>
        <input
          type="search"
          className={styles.toolbarBtn}
          style={{ flex: 1, minWidth: 0 }}
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Filter…"
        />
        <select
          className={styles.toolbarBtn}
          value={minLength}
          onChange={(e) => setMinLength(Number(e.target.value))}
          title="Minimum string length"
        >
          {MIN_LENGTHS.map((n) => (
            <option key={n} value={n}>
              ≥ {n}
            </option>
          ))}
        </select>
        <button
          type="button"
          className={styles.toolbarBtn}
          onClick={() => openPanel('binary')}
          title="Jump back to HEX view"
        >
          Open HEX
        </button>
        <span className={styles.dirtyStatus}>
          {loading
            ? 'Scanning…'
            : strings == null
              ? ''
              : `${filtered.length} / ${strings.length}`}
        </span>
      </div>
      {error != null ? (
        <p className={styles.messageError} role="alert">
          {error}
        </p>
      ) : null}
      <div
        ref={scrollRef}
        onScroll={(e: UIEvent<HTMLDivElement>) => setScrollTop(e.currentTarget.scrollTop)}
        style={{
          flex: 1,
          minHeight: 0,
          overflow: 'auto',
          fontFamily: 'var(--font-family-mono)',
          fontSize: 13,
        }}
      >
        <div style={{ height: filtered.length * ROW_PX, position: 'relative' }}>
          <div style={{ position: 'absolute', top: firstVisibleRow * ROW_PX, left: 0, right: 0 }}>
            {visibleStrings.map((s) => (
              <div
                key={`${s.offset}-${s.length}`}
                style={{
                  display: 'grid',
                  gridTemplateColumns: '7rem 4rem 1fr',
                  alignItems: 'center',
                  gap: '0.5rem',
                  height: ROW_PX,
                  boxSizing: 'border-box',
                  padding: '0 0.5rem',
                  borderBottom: '1px solid rgba(255,255,255,0.04)',
                  whiteSpace: 'nowrap',
                }}
                title={s.text}
              >
                <span style={{ opacity: 0.7 }}>0x{s.offset.toString(16).padStart(8, '0')}</span>
                <span style={{ opacity: 0.6 }}>{s.length}</span>
                <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{s.text}</span>
              </div>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}
