import { useCallback, useEffect, useState } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import { gitStatus, type GitFileStatus, type GitStatus } from '@infrastructure/tauri/bridge';

import { useIdeSession } from './IdeSessionContext';
import { FileIcon } from './fileIcons';

import styles from './GitPanel.module.css';

/** Single-letter status badge (M/A/D/U/…) for a changed file. */
function statusBadge(f: GitFileStatus): string {
  if (f.untracked) return 'U';
  const code = (f.staged ? f.indexStatus : f.workStatus).trim();
  return code === '' ? '•' : code;
}

function dirOf(rel: string): string {
  const i = Math.max(rel.lastIndexOf('/'), rel.lastIndexOf('\\'));
  return i > 0 ? rel.slice(0, i) : '';
}

/**
 * Source Control view — the first real "pack" built on the side-panel seam (see
 * documentation/architecture/PLUGINS.md). Lists the workspace repo's changed
 * files via the `git_status` Rust command; click a file to open it. Stage /
 * commit / diff come in later iterations.
 */
export function GitPanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const { openFileFromWorkspace, fileTreeRevision } = useIdeSession();
  const [status, setStatus] = useState<GitStatus | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    const root = workspaceRoot?.path;
    if (root == null || root === '') {
      setStatus(null);
      return;
    }
    setBusy(true);
    setError(null);
    try {
      setStatus(await gitStatus(root));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBusy(false);
    }
  }, [workspaceRoot?.path]);

  // Refresh on open, on workspace switch, and whenever the tree changes
  // (create / delete / rename / save all bump fileTreeRevision).
  useEffect(() => {
    void refresh();
  }, [refresh, fileTreeRevision]);

  if (workspaceRoot == null || workspaceRoot.path === '') {
    return (
      <div className={styles.stateBox} role="status">
        <p className={styles.stateLine}>No workspace folder open.</p>
      </div>
    );
  }

  const staged = status?.files.filter((f) => f.staged) ?? [];
  const changes = status?.files.filter((f) => !f.staged) ?? [];

  const fileRow = (f: GitFileStatus) => (
    <button
      key={f.path}
      type="button"
      className={styles.fileRow}
      title={f.path}
      onClick={() => void openFileFromWorkspace(f.absPath)}
    >
      <FileIcon name={f.name} />
      <span className={styles.fileName}>{f.name}</span>
      <span className={styles.dir}>{dirOf(f.path)}</span>
      <span className={`${styles.badge} ${f.untracked ? styles.badgeUntracked : ''}`}>
        {statusBadge(f)}
      </span>
    </button>
  );

  return (
    <div className={styles.section}>
      <div className={styles.header}>
        <span className={styles.headerName}>Source Control</span>
        {status?.branch != null ? <span className={styles.branch}>{status.branch}</span> : null}
        <div className={styles.headerActions}>
          <button
            type="button"
            className={styles.headerBtn}
            title="Refresh"
            aria-label="Refresh git status"
            onClick={() => void refresh()}
          >
            ⟳
          </button>
        </div>
      </div>
      <div className={styles.body}>
        {error != null ? (
          <p className={styles.stateLine} role="alert">
            {error}
          </p>
        ) : status != null && !status.isRepo ? (
          <div className={styles.stateBox} role="status">
            <p className={styles.stateLine}>Not a git repository.</p>
            <p className={styles.stateHint}>Run “git init” here to start tracking changes.</p>
          </div>
        ) : status != null && status.files.length === 0 ? (
          <div className={styles.stateBox} role="status">
            <p className={styles.stateLine}>{busy ? 'Loading…' : 'No changes.'}</p>
          </div>
        ) : (
          <>
            {staged.length > 0 ? (
              <>
                <div className={styles.groupLabel}>Staged Changes</div>
                {staged.map(fileRow)}
              </>
            ) : null}
            {changes.length > 0 ? (
              <>
                <div className={styles.groupLabel}>Changes</div>
                {changes.map(fileRow)}
              </>
            ) : null}
          </>
        )}
      </div>
    </div>
  );
}
