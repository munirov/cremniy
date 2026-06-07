import { useCallback, useEffect, useState } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import {
  gitInit,
  gitRepos,
  gitStatus,
  type GitRepoRef,
  type GitStatus,
} from '@infrastructure/tauri/bridge';

import { runAgentCommand } from '@shared/agent/agentBridge';

import { useIdeSession } from './IdeSessionContext';
import { ChevronIcon } from './fileIcons';
import { GitRepoSection } from './GitRepoSection';

import styles from './GitPanel.module.css';

/**
 * Source Control view — the first real "pack" on the side-panel seam (see
 * documentation/architecture/PLUGINS.md). Discovers the git repos under the
 * workspace. One repo → its changes directly; multiple → two collapsible
 * sections like VS Code: REPOSITORIES (compact overview) and CHANGES (each repo
 * expands to its own commit box + groups + files), so commit is always per-repo.
 */
export function GitPanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const { fileTreeRevision, bumpFileTreeRevision } = useIdeSession();
  const [repos, setRepos] = useState<GitRepoRef[] | null>(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [reposOpen, setReposOpen] = useState(true);
  const [changesOpen, setChangesOpen] = useState(true);
  const [overview, setOverview] = useState<Record<string, GitStatus>>({});

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

  // Repo discovery walks the whole workspace tree, so it must NOT run on every
  // refresh tick — only on workspace change (discover's own dep) and the manual
  // Rescan button. Per-repo status (overview below + each section) stays live
  // via fileTreeRevision, which is what actually changes as you edit files.
  useEffect(() => {
    void discover();
  }, [discover]);

  // Lightweight per-repo status for the REPOSITORIES overview (branch + counts).
  // CHANGES loads its own full status per section.
  useEffect(() => {
    if (repos == null || repos.length === 0) {
      return;
    }
    let cancelled = false;
    void Promise.all(
      repos.map((r) =>
        gitStatus(r.path)
          .then((s) => [r.path, s] as const)
          .catch(() => null),
      ),
    ).then((pairs) => {
      if (cancelled) return;
      const m: Record<string, GitStatus> = {};
      for (const pair of pairs) {
        if (pair) m[pair[0]] = pair[1];
      }
      setOverview(m);
    });
    return () => {
      cancelled = true;
    };
  }, [repos, fileTreeRevision]);

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

  // Always show the two collapsible sections (REPOSITORIES + CHANGES) — even for
  // a single repo, per the requested layout.
  return (
    <div className={styles.section}>
      <div className={styles.scrollArea}>
        <div className={styles.sectionHeader}>
          <button
            type="button"
            className={styles.sectionToggle}
            onClick={() => setReposOpen((v) => !v)}
          >
            <ChevronIcon open={reposOpen} />
            <span className={styles.sectionTitle}>Repositories</span>
            <span className={styles.count}>{repos.length}</span>
          </button>
          <div className={styles.headerActions}>
            <button
              type="button"
              className={styles.headerBtn}
              title="Advanced Git (branches, merge, rebase, stash, history, remotes)"
              aria-label="Advanced Git"
              onClick={() => void runAgentCommand('dialog.openAdvancedGit')}
            >
              ⎇
            </button>
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
        {reposOpen
          ? repos.map((r) => {
              const s = overview[r.path];
              return (
                <div key={r.path} className={styles.repoListRow} title={r.path}>
                  <span className={styles.repoListName}>{r.name}</span>
                  {s?.branch != null ? (
                    <span className={styles.repoListBranch}>
                      ⎇ {s.branch}
                      {s.behind > 0 ? ` ↓${s.behind}` : ''}
                      {s.ahead > 0 ? ` ↑${s.ahead}` : ''}
                    </span>
                  ) : (
                    <span className={styles.repoListBranch} />
                  )}
                  {s != null && s.files.length > 0 ? (
                    <span className={styles.count}>{s.files.length}</span>
                  ) : null}
                </div>
              );
            })
          : null}

        <div className={styles.sectionHeader}>
          <button
            type="button"
            className={styles.sectionToggle}
            onClick={() => setChangesOpen((v) => !v)}
          >
            <ChevronIcon open={changesOpen} />
            <span className={styles.sectionTitle}>Changes</span>
          </button>
        </div>
        {changesOpen
          ? repos.map((r, i) => (
              <GitRepoSection
                key={r.path}
                repoPath={r.path}
                repoName={r.name}
                multi
                defaultExpanded={i === 0}
              />
            ))
          : null}
      </div>
    </div>
  );
}
