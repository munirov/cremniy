import { useEffect, useMemo, useState } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import { extractAsciiStrings, filterStrings, type ExtractedString } from '@domain/strings/stringExtraction';
import { readWorkspaceFileBytesForAnalysis } from '@infrastructure/tauri/bridge';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './BinaryToolPanel.module.css';

const MIN_LENGTHS = [4, 5, 6, 8, 10, 16];

export function StringsToolPanel() {
  const { activeFilePath, fileContentRevision } = useIdeSession();
  const { selectToolTab } = useToolDock();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';

  const [strings, setStrings] = useState<ExtractedString[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [query, setQuery] = useState('');
  const [minLength, setMinLength] = useState(4);

  useEffect(() => {
    if (activeFilePath == null || activeFilePath === '' || workspacePath === '') {
      setStrings(null);
      setError(null);
      return;
    }
    let cancelled = false;
    setLoading(true);
    setError(null);
    void readWorkspaceFileBytesForAnalysis(workspacePath, activeFilePath)
      .then((bytes) => {
        if (cancelled) return;
        const extracted = extractAsciiStrings(bytes, minLength);
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

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a file to extract ASCII strings.</p>
      </section>
    );
  }

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
          onClick={() => selectToolTab('binary')}
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
        style={{
          flex: 1,
          minHeight: 0,
          overflow: 'auto',
          fontFamily: 'var(--font-family-mono)',
          fontSize: 13,
        }}
      >
        {filtered.map((s) => (
          <div
            key={`${s.offset}-${s.length}`}
            style={{
              display: 'grid',
              gridTemplateColumns: '7rem 4rem 1fr',
              gap: '0.5rem',
              padding: '2px 0.5rem',
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
    </section>
  );
}
