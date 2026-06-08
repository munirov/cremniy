import { useEffect, useMemo, useState } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import { analyzeBinary, type BinaryAnalysisDto, type BinarySymbolDto } from '@infrastructure/tauri/bridge';
import { useSetBinarySelection } from '@boundary/workspace/BinarySelectionContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './BinaryToolPanel.module.css';

export function SymbolTableToolPanel() {
  const { activeFilePath, fileContentRevision } = useIdeSession();
  const setSelection = useSetBinarySelection();
  const { selectToolTab } = useToolDock();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';

  const [analysis, setAnalysis] = useState<BinaryAnalysisDto | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [query, setQuery] = useState('');
  const [source, setSource] = useState<'all' | 'import' | 'export' | '.dynsym' | '.symtab'>('all');

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

  const filtered = useMemo(() => {
    if (analysis == null) return [];
    const q = query.trim().toLowerCase();
    return analysis.symbols.filter((s) => {
      if (source !== 'all') {
        if (source === 'import' && !s.source.toLowerCase().includes('import')) return false;
        if (source === 'export' && !s.source.toLowerCase().includes('export')) return false;
        if ((source === '.dynsym' || source === '.symtab') && !s.source.includes(source)) return false;
      }
      if (q === '') return true;
      return (
        s.name.toLowerCase().includes(q) ||
        s.address.toLowerCase().includes(q) ||
        s.source.toLowerCase().includes(q)
      );
    });
  }, [analysis, query, source]);

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a file to inspect its symbol table.</p>
      </section>
    );
  }

  return (
    <section className={styles.root} aria-label="Symbol table">
      <p className={styles.subtitle} title={activeFilePath}>
        {fileNameFromPath(activeFilePath)}
        {analysis != null ? `  ·  ${analysis.format} ${analysis.bitness ? `${analysis.bitness}-bit` : ''}` : ''}
      </p>
      <div className={styles.toolbar}>
        <input
          type="search"
          className={styles.toolbarBtn}
          style={{ flex: 1, minWidth: 0 }}
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Filter name / address / source…"
        />
        <select
          className={styles.toolbarBtn}
          value={source}
          onChange={(e) => setSource(e.target.value as typeof source)}
        >
          <option value="all">All sources</option>
          <option value="import">Imports</option>
          <option value="export">Exports</option>
          <option value=".dynsym">.dynsym</option>
          <option value=".symtab">.symtab</option>
        </select>
        <span className={styles.dirtyStatus}>
          {loading
            ? 'Analyzing…'
            : analysis == null
              ? ''
              : `${filtered.length} / ${analysis.symbols.length}`}
        </span>
      </div>
      {error != null ? (
        <p className={styles.messageError} role="alert">
          {error}
        </p>
      ) : null}
      {analysis != null && analysis.symbols.length === 0 ? (
        <p className={styles.message}>
          No symbols. (Raw / stripped binary, or format not supported yet.)
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
        {filtered.map((sym, i) => (
          <SymbolRow
            key={`${sym.source}-${sym.address}-${sym.name}-${i}`}
            sym={sym}
            onJump={() => {
              const addr = Number.parseInt(sym.address, 16);
              if (Number.isFinite(addr)) {
                setSelection({ offset: addr, length: sym.size ?? 1, source: 'symbols' });
                selectToolTab('binary');
              }
            }}
          />
        ))}
      </div>
    </section>
  );
}

function SymbolRow({ sym, onJump }: { sym: BinarySymbolDto; onJump: () => void }) {
  const kindColor: Record<string, string> = {
    func: '#9cdcfe',
    object: '#dcdcaa',
    section: '#c586c0',
    notype: '#999',
  };
  return (
    <div
      role="button"
      tabIndex={0}
      onClick={onJump}
      onKeyDown={(e) => {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          onJump();
        }
      }}
      style={{
        display: 'grid',
        gridTemplateColumns: '14rem 4rem 7rem 1fr 14rem',
        gap: '0.5rem',
        padding: '2px 0.5rem',
        borderBottom: '1px solid rgba(255,255,255,0.04)',
        whiteSpace: 'nowrap',
        cursor: 'pointer',
      }}
      title={`Jump to ${sym.address} — ${sym.name}`}
    >
      <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{sym.name}</span>
      <span style={{ color: kindColor[sym.kind] ?? '#bbb' }}>{sym.kind}</span>
      <span style={{ opacity: 0.7 }}>{sym.binding}</span>
      <span style={{ opacity: 0.65, overflow: 'hidden', textOverflow: 'ellipsis' }}>{sym.source}</span>
      <span style={{ opacity: 0.85 }}>{sym.address}</span>
    </div>
  );
}
