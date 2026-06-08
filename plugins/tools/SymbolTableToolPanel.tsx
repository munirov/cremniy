import { useEffect, useLayoutEffect, useMemo, useRef, useState, type UIEvent } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import { analyzeBinary, type BinaryAnalysisDto, type BinarySymbolDto } from '@infrastructure/tauri/bridge';
import { useSetBinarySelection } from '@boundary/workspace/BinarySelectionContext';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './BinaryToolPanel.module.css';

const ROW_PX = 22;
const ROW_BUFFER = 6;

export function SymbolTableToolPanel() {
  const { activeFilePath, fileContentRevision, openPanel } = useIdeSession();
  const setSelection = useSetBinarySelection();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';

  const [analysis, setAnalysis] = useState<BinaryAnalysisDto | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [query, setQuery] = useState('');
  const [source, setSource] = useState<'all' | 'import' | 'export' | '.dynsym' | '.symtab'>('all');

  const scrollRef = useRef<HTMLDivElement>(null);
  const [scrollTop, setScrollTop] = useState(0);
  const [viewportHeight, setViewportHeight] = useState(0);

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

  // Virtualize: render only the visible window of rows so a 5000-symbol result
  // stays a few dozen DOM nodes (mirrors StringsToolPanel).
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
  }, [activeFilePath, query, source]);

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a file to inspect its symbol table.</p>
      </section>
    );
  }

  const firstVisibleRow = Math.max(0, Math.floor(scrollTop / ROW_PX) - ROW_BUFFER);
  const visibleRowCount = Math.ceil((viewportHeight || ROW_PX) / ROW_PX) + ROW_BUFFER * 2;
  const lastVisibleRow = Math.min(filtered.length, firstVisibleRow + visibleRowCount);
  const visibleSymbols = filtered.slice(firstVisibleRow, lastVisibleRow);
  const capped = analysis != null && analysis.symbolsTotal > analysis.symbols.length;

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
      {capped ? (
        <p className={styles.message} style={{ opacity: 0.55, fontSize: 11 }}>
          Showing first {analysis!.symbols.length.toLocaleString()} of{' '}
          {analysis!.symbolsTotal.toLocaleString()} symbols.
        </p>
      ) : null}
      {analysis != null && analysis.symbols.length === 0 ? (
        <p className={styles.message}>
          No symbols. (Raw / stripped binary, or format not supported yet.)
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
            {visibleSymbols.map((sym, i) => (
              <SymbolRow
                key={`${sym.source}-${sym.address}-${sym.name}-${firstVisibleRow + i}`}
                sym={sym}
                onJump={() => {
                  const addr = Number.parseInt(sym.address, 16);
                  if (Number.isFinite(addr)) {
                    setSelection({ offset: addr, length: sym.size ?? 1, source: 'symbols' });
                    openPanel('binary');
                  }
                }}
              />
            ))}
          </div>
        </div>
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
        alignItems: 'center',
        height: ROW_PX,
        boxSizing: 'border-box',
        padding: '0 0.5rem',
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
