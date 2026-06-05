import { useEffect, useState } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';
import {
  analyzeBinary,
  type BinaryAnalysisDto,
  type BinarySectionDto,
} from '@infrastructure/tauri/bridge';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './BinaryToolPanel.module.css';

/**
 * Resources Tool Tab (Qt parity: PE .rsrc viewer).
 *
 * Full IMAGE_RESOURCE_DIRECTORY parsing (icons, version info, dialogs) lives
 * in a future iteration — goblin doesn't expose the tree directly, the parse
 * is recursive and format-specific. Until then we surface the relevant
 * sections (.rsrc / .reloc / .pdata / .edata / .idata) extracted by
 * analyze_binary so the user at least sees their VMA / size / flags.
 */
const RESOURCE_LIKE_SECTIONS = new Set(['.rsrc', '.idata', '.edata', '.reloc', '.pdata', '.tls']);

export function ResourcesToolPanel() {
  const { activeFilePath, fileContentRevision } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  const workspacePath = workspaceRoot?.path?.trim() ?? '';

  const [analysis, setAnalysis] = useState<BinaryAnalysisDto | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

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

  if (activeFilePath == null || activeFilePath === '') {
    return (
      <section className={styles.root}>
        <p className={styles.message}>Open a PE binary to inspect its resources.</p>
      </section>
    );
  }

  const resourceSections =
    analysis?.sections.filter((s) => RESOURCE_LIKE_SECTIONS.has(s.name)) ?? [];

  return (
    <section className={styles.root} aria-label="Resources">
      <p className={styles.subtitle} title={activeFilePath}>
        {fileNameFromPath(activeFilePath)}
        {analysis != null ? `  ·  ${analysis.format}` : ''}
      </p>
      {loading ? <p className={styles.message}>Loading…</p> : null}
      {error != null ? (
        <p className={styles.messageError} role="alert">
          {error}
        </p>
      ) : null}
      {analysis != null && analysis.format !== 'PE' ? (
        <p className={styles.message}>Resources are only supported for PE binaries.</p>
      ) : null}
      {analysis != null && analysis.format === 'PE' && resourceSections.length === 0 ? (
        <p className={styles.message}>No resource sections found.</p>
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
        {resourceSections.map((sec, i) => (
          <ResourceSectionRow key={`${sec.name}-${i}`} sec={sec} />
        ))}
      </div>
      {resourceSections.length > 0 ? (
        <p className={styles.message} style={{ opacity: 0.6, fontSize: 11 }}>
          Detailed .rsrc tree (icons, version info, dialogs) is on the roadmap.
        </p>
      ) : null}
    </section>
  );
}

function ResourceSectionRow({ sec }: { sec: BinarySectionDto }) {
  const friendly: Record<string, string> = {
    '.rsrc': 'Embedded resources (icons / version / dialogs)',
    '.idata': 'Import directory',
    '.edata': 'Export directory',
    '.reloc': 'Base relocations',
    '.pdata': 'Exception handlers',
    '.tls': 'Thread-local storage',
  };
  return (
    <div
      style={{
        display: 'grid',
        gridTemplateColumns: '6rem 14rem 6rem 1fr',
        gap: '0.5rem',
        padding: '4px 0.5rem',
        borderBottom: '1px solid rgba(255,255,255,0.04)',
        whiteSpace: 'nowrap',
      }}
      title={friendly[sec.name] ?? sec.name}
    >
      <span style={{ color: '#c586c0', fontWeight: 600 }}>{sec.name}</span>
      <span style={{ opacity: 0.85 }}>{sec.vma}</span>
      <span style={{ opacity: 0.7 }}>{sec.size.toLocaleString()}b</span>
      <span style={{ opacity: 0.65, overflow: 'hidden', textOverflow: 'ellipsis' }}>
        {friendly[sec.name] ?? '—'}
      </span>
    </div>
  );
}
