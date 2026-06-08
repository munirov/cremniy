import { useEffect, useLayoutEffect, useMemo, useRef, useState, type UIEvent } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import { analyzeBinary, type BinaryAnalysisDto, type BinarySectionDto } from '@infrastructure/tauri/bridge';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './BinaryToolPanel.module.css';

const ROW_PX = 30;
const ROW_BUFFER = 6;

export function MemoryMapToolPanel() {
  const { activeFilePath, fileContentRevision } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';

  const [analysis, setAnalysis] = useState<BinaryAnalysisDto | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

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

  const maxSize = useMemo(() => {
    if (analysis == null || analysis.sections.length === 0) return 1;
    return analysis.sections.reduce((m, s) => Math.max(m, s.size), 1);
  }, [analysis]);

  // Virtualize: render only the visible window of rows (mirrors StringsToolPanel).
  // Section counts are usually small, but a synthetic/odd binary can carry many.
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
  }, [activeFilePath]);

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a binary to see its memory map.</p>
      </section>
    );
  }

  const sections = analysis?.sections ?? [];
  const firstVisibleRow = Math.max(0, Math.floor(scrollTop / ROW_PX) - ROW_BUFFER);
  const visibleRowCount = Math.ceil((viewportHeight || ROW_PX) / ROW_PX) + ROW_BUFFER * 2;
  const lastVisibleRow = Math.min(sections.length, firstVisibleRow + visibleRowCount);
  const visibleSections = sections.slice(firstVisibleRow, lastVisibleRow);
  const capped = analysis != null && analysis.sectionsTotal > analysis.sections.length;

  return (
    <section className={styles.root} aria-label="Memory map">
      <p className={styles.subtitle} title={activeFilePath}>
        {fileNameFromPath(activeFilePath)}
        {analysis != null ? `  ·  ${analysis.format} ${analysis.bitness ? `${analysis.bitness}-bit` : ''}` : ''}
      </p>
      {loading ? <p className={styles.message}>Loading…</p> : null}
      {error != null ? (
        <p className={styles.messageError} role="alert">
          {error}
        </p>
      ) : null}
      {capped ? (
        <p className={styles.message} style={{ opacity: 0.55, fontSize: 11 }}>
          Showing first {analysis!.sections.length.toLocaleString()} of{' '}
          {analysis!.sectionsTotal.toLocaleString()} sections.
        </p>
      ) : null}
      {analysis != null && analysis.sections.length === 0 ? (
        <p className={styles.message}>No sections (raw or stripped binary).</p>
      ) : null}
      <div
        ref={scrollRef}
        onScroll={(e: UIEvent<HTMLDivElement>) => setScrollTop(e.currentTarget.scrollTop)}
        style={{
          flex: 1,
          minHeight: 0,
          overflow: 'auto',
          padding: '0.25rem 0',
          fontFamily: 'var(--font-family-mono)',
          fontSize: 13,
        }}
      >
        <div style={{ height: sections.length * ROW_PX, position: 'relative' }}>
          <div style={{ position: 'absolute', top: firstVisibleRow * ROW_PX, left: 0, right: 0 }}>
            {visibleSections.map((sec, i) => (
              <SectionRow key={`${sec.name}-${sec.vma}-${firstVisibleRow + i}`} sec={sec} maxSize={maxSize} />
            ))}
          </div>
        </div>
      </div>
    </section>
  );
}

function SectionRow({ sec, maxSize }: { sec: BinarySectionDto; maxSize: number }) {
  const widthPct = Math.max(2, Math.min(100, (sec.size / maxSize) * 100));
  const bg = sec.isExecutable
    ? 'rgba(244,71,71,0.4)'
    : sec.isWritable
      ? 'rgba(215,186,125,0.4)'
      : sec.isReadable
        ? 'rgba(108,217,122,0.35)'
        : 'rgba(150,150,150,0.3)';
  const perms = `${sec.isReadable ? 'R' : '-'}${sec.isWritable ? 'W' : '-'}${sec.isExecutable ? 'X' : '-'}`;
  return (
    <div
      style={{
        display: 'grid',
        gridTemplateColumns: '12rem 12rem 4rem 1fr',
        gap: '0.5rem',
        alignItems: 'center',
        height: ROW_PX,
        boxSizing: 'border-box',
        padding: '0 0.5rem',
        borderBottom: '1px solid rgba(255,255,255,0.04)',
        whiteSpace: 'nowrap',
      }}
      title={`${sec.name}  VMA ${sec.vma}  size ${sec.size}  ${perms}`}
    >
      <span style={{ overflow: 'hidden', textOverflow: 'ellipsis' }}>{sec.name}</span>
      <span style={{ opacity: 0.75 }}>{sec.vma}</span>
      <span style={{ opacity: 0.75 }}>{perms}</span>
      <div
        style={{
          position: 'relative',
          height: '0.85rem',
          background: 'rgba(255,255,255,0.04)',
          borderRadius: 2,
          overflow: 'hidden',
        }}
      >
        <div
          style={{
            width: `${widthPct}%`,
            height: '100%',
            background: bg,
          }}
        />
        <span
          style={{
            position: 'absolute',
            top: 0,
            left: '0.4rem',
            fontSize: 11,
            opacity: 0.85,
          }}
        >
          {sec.size.toLocaleString()} bytes
        </span>
      </div>
    </div>
  );
}
