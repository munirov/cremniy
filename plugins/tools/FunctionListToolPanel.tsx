import { useEffect, useMemo, useState } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import { analyzeBinary, type BinaryAnalysisDto } from '@infrastructure/tauri/bridge';
import { useSetBinarySelection } from '@boundary/workspace/BinarySelectionContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './BinaryToolPanel.module.css';

/**
 * Function List tool tab (Qt parity). Distinct from the disassembler's
 * inline jump-to-function dropdown — this one is a full searchable table of
 * function symbols extracted from the binary, independent of the disasm run.
 */
export function FunctionListToolPanel() {
  const { activeFilePath, fileContentRevision, openPanel } = useIdeSession();
  const setSelection = useSetBinarySelection();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';

  const [analysis, setAnalysis] = useState<BinaryAnalysisDto | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [query, setQuery] = useState('');

  useEffect(() => {
    if (activeFilePath == null || activeFilePath === '' || workspacePath === '') {
      setAnalysis(null);
      setError(null);
      return;
    }
    let cancelled = false;
    setLoading(true);
    setError(null);
    analyzeBinary(workspacePath, activeFilePath)
      .then((res) => {
        if (!cancelled) setAnalysis(res);
      })
      .catch((e: unknown) => {
        if (!cancelled) setError(e instanceof Error ? e.message : String(e));
      })
      .finally(() => {
        if (!cancelled) setLoading(false);
      });
    return () => {
      cancelled = true;
    };
  }, [activeFilePath, workspacePath, fileContentRevision]);

  const functions = useMemo(() => {
    if (analysis == null) return [];
    const seen = new Set<string>();
    return analysis.symbols.filter((s) => {
      if (s.kind !== 'func') return false;
      const key = `${s.address}::${s.name}`;
      if (seen.has(key)) return false;
      seen.add(key);
      return true;
    });
  }, [analysis]);

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    if (q === '') return functions;
    return functions.filter(
      (f) => f.name.toLowerCase().includes(q) || f.address.toLowerCase().includes(q),
    );
  }, [functions, query]);

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a binary to list its functions.</p>
      </section>
    );
  }

  return (
    <section className={styles.root} aria-label="Function list">
      <p className={styles.subtitle} title={activeFilePath}>
        {fileNameFromPath(activeFilePath)}
        {analysis != null ? `  ·  ${analysis.format}` : ''}
      </p>
      <div className={styles.toolbar}>
        <input
          type="search"
          className={styles.toolbarBtn}
          style={{ flex: 1, minWidth: 0 }}
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Filter functions…"
        />
        <button
          type="button"
          className={styles.toolbarBtn}
          onClick={() => openPanel('disassembler')}
          title="Switch to Disassembler tab"
        >
          Open Disasm
        </button>
        <span className={styles.dirtyStatus}>
          {loading ? 'Analyzing…' : `${filtered.length} / ${functions.length}`}
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
        {filtered.map((f, i) => (
          <div
            key={`${f.address}-${f.name}-${i}`}
            role="button"
            tabIndex={0}
            onClick={() => {
              const addr = Number.parseInt(f.address, 16);
              if (Number.isFinite(addr)) {
                setSelection({ offset: addr, length: f.size ?? 1, source: 'functions' });
                openPanel('binary');
              }
            }}
            style={{
              display: 'grid',
              gridTemplateColumns: '14rem 4rem 1fr',
              gap: '0.5rem',
              padding: '2px 0.5rem',
              borderBottom: '1px solid rgba(255,255,255,0.04)',
              whiteSpace: 'nowrap',
              cursor: 'pointer',
            }}
            title={`Jump to ${f.address} — ${f.name}`}
          >
            <span style={{ opacity: 0.85 }}>{f.address}</span>
            <span style={{ opacity: 0.65 }}>{f.size ?? '?'}</span>
            <span
              style={{ overflow: 'hidden', textOverflow: 'ellipsis', color: '#9cdcfe' }}
            >
              {f.name}
            </span>
          </div>
        ))}
      </div>
    </section>
  );
}
