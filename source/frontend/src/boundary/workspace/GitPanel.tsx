import { useCallback, useEffect, useState } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import { gitInit, gitRepos, type GitRepoRef } from '@infrastructure/tauri/bridge';

import { useIdeSession } from './IdeSessionContext';
import { GitRepoSection } from './GitRepoSection';

import styles from './GitPanel.module.css';

/**
 * Source Control view — the first real "pack" on the side-panel seam (see
 * documentation/architecture/PLUGINS.md). Discovers the git repos under the
 * workspace and renders each one's changes (commit / stage / branch / discard)
 * via GitRepoSection — a single repo directly, multiple repos nested under a
 * Repositories list, so the commit button always commits its own repo.
 */
export function GitPanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const { fileTreeRevision, bumpFileTreeRevision } = useIdeSession();
  const [repos, setRepos] = useState<GitRepoRef[] | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const discover = useCallback(async () => {
    const root = workspaceRoot?.path;
    if (root == null || root === '') {
      setRepos(null);
      return;
    }
    setBusy(true);
    setError(null);
    try {
      setRepos(await gitRepos(root));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setRepos([]);
    } finally {
      setBusy(false);
    }
  }, [workspaceRoot?.path]);

  useEffect(() => {
    void discover();
  }, [discover, fileTreeRevision]);

  if (workspaceRoot == null || workspaceRoot.path === '') {
    return (
      <div className={styles.stateBox} role="status">
        <p className={styles.stateLine}>No workspace folder open.</p>
      </div>
    );
  }

  if (repos == null) {
    return (
      <div className={styles.stateBox} role="status" aria-busy={busy}>
        <p className={styles.stateLine}>{busy ? 'Scanning for repositories…' : ''}</p>
      </div>
    );
  }

  // No repos found → offer to init the workspace root.
  if (repos.length === 0) {
    return (
      <div className={styles.section}>
        <div className={styles.header}>
          <span className={styles.headerName}>Source Control</span>
        </div>
        <div className={styles.body}>
          {error != null ? (
            <p className={styles.errorLine} role="alert">
              {error}
            </p>
          ) : null}
          <div className={styles.stateBox} role="status">
            <p className={styles.stateLine}>No git repository here.</p>
            <button
              type="button"
              className={styles.commitBtn}
              disabled={busy}
              onClick={() => {
                void gitInit(workspaceRoot.path)
                  .then(discover)
                  .then(() => bumpFileTreeRevision())
                  .catch((e) => setError(e instanceof Error ? e.message : String(e)));
              }}
            >
              Initialize Repository
            </button>
          </div>
        </div>
      </div>
    );
  }

  // Single repo → render it directly (no Repositories list, like VS Code).
  if (repos.length === 1) {
    const r = repos[0]!;
    return <GitRepoSection repoPath={r.path} repoName={r.name} multi={false} />;
  }

  // Multiple repos → a Repositories header + one nested section per repo.
  return (
    <div className={styles.section}>
      <div className={styles.header}>
        <span className={styles.headerName}>Repositories</span>
        <span className={styles.count}>{repos.length}</span>
        <div className={styles.headerActions}>
          <button
            type="button"
            className={styles.headerBtn}
            title="Rescan repositories"
            aria-label="Rescan repositories"
            onClick={() => void discover()}
          >
            ⟳
          </button>
        </div>
      </div>
      <div className={styles.body}>
        {repos.map((r, i) => (
          <GitRepoSection
            key={r.path}
            repoPath={r.path}
            repoName={r.name}
            multi
            defaultExpanded={i === 0}
          />
        ))}
      </div>
    </div>
  );
}
