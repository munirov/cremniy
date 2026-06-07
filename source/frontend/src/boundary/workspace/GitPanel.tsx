import { useCallback, useEffect, useState } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import {
  gitBranches,
  gitCheckout,
  gitCommit,
  gitCreateBranch,
  gitDiscard,
  gitInit,
  gitPull,
  gitPush,
  gitStage,
  gitStatus,
  gitUnstage,
  revealInFileManager,
  type GitFileStatus,
  type GitStatus,
} from '@infrastructure/tauri/bridge';

import { useIdeSession } from './IdeSessionContext';
import { FileIcon, FolderIcon } from './fileIcons';

import styles from './GitPanel.module.css';

/** Single-letter status badge (M/A/D/U/…) for a changed file. */
function statusBadge(f: GitFileStatus): string {
  if (f.untracked) return 'U';
  const code = (f.staged ? f.indexStatus : f.workStatus).trim();
  return code === '' ? '•' : code;
}

/** Full status word for the badge tooltip — A/M/D/U are otherwise cryptic. */
function statusLabel(f: GitFileStatus): string {
  if (f.untracked) return 'Untracked';
  const code = (f.staged ? f.indexStatus : f.workStatus).trim();
  switch (code) {
    case 'M':
      return 'Modified';
    case 'A':
      return 'Added';
    case 'D':
      return 'Deleted';
    case 'R':
      return 'Renamed';
    case 'C':
      return 'Copied';
    case 'U':
      return 'Conflict';
    default:
      return 'Changed';
  }
}

function dirOf(rel: string): string {
  const i = Math.max(rel.lastIndexOf('/'), rel.lastIndexOf('\\'));
  return i > 0 ? rel.slice(0, i) : '';
}

/**
 * Source Control view — the first real "pack" built on the side-panel seam (see
 * documentation/architecture/PLUGINS.md). Full working-tree git through the UI:
 * init, stage/unstage (pick what to commit), and commit with a message — no
 * command line needed. Diff-on-click, push/pull, and branch switching come next.
 */
export function GitPanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const { openFileFromWorkspace, fileTreeRevision, bumpFileTreeRevision } = useIdeSession();
  const [status, setStatus] = useState<GitStatus | null>(null);
  const [message, setMessage] = useState('');
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [commitMenu, setCommitMenu] = useState(false);
  const [branchMenu, setBranchMenu] = useState(false);
  const [branches, setBranches] = useState<string[]>([]);
  const [newBranchMode, setNewBranchMode] = useState(false);
  const [newBranchName, setNewBranchName] = useState('');

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

  // Run a git mutation, surface its error, then refresh status + tree decorations.
  const runGit = useCallback(
    async (op: (root: string) => Promise<void>) => {
      const root = workspaceRoot?.path;
      if (root == null || root === '') {
        return;
      }
      setError(null);
      try {
        await op(root);
      } catch (e) {
        setError(e instanceof Error ? e.message : String(e));
      }
      await refresh();
      bumpFileTreeRevision();
    },
    [workspaceRoot?.path, refresh, bumpFileTreeRevision],
  );

  if (workspaceRoot == null || workspaceRoot.path === '') {
    return (
      <div className={styles.stateBox} role="status">
        <p className={styles.stateLine}>No workspace folder open.</p>
      </div>
    );
  }

  const staged = status?.files.filter((f) => f.staged) ?? [];
  const changes = status?.files.filter((f) => !f.staged) ?? [];
  const isRepo = status?.isRepo ?? false;
  const hasMessage = message.trim() !== '';

  // One entry-point for every commit variant: `all` stages all changes first,
  // `push` pushes after, `amend` rewrites the last commit.
  const doCommit = (opts: { all?: boolean; push?: boolean; amend?: boolean } = {}) => {
    setCommitMenu(false);
    void runGit(async (r) => {
      if (opts.all && changes.length > 0) {
        await gitStage(
          r,
          changes.map((f) => f.path),
        );
      }
      await gitCommit(r, message, opts.amend ?? false);
      if (opts.push) {
        await gitPush(r);
      }
      setMessage('');
    });
  };

  const openBranchMenu = () => {
    const root = workspaceRoot?.path;
    if (root == null || root === '') return;
    setNewBranchMode(false);
    setNewBranchName('');
    void gitBranches(root)
      .then(setBranches)
      .catch(() => setBranches([]));
    setBranchMenu((v) => !v);
  };

  const checkout = (b: string) => {
    setBranchMenu(false);
    void runGit((r) => gitCheckout(r, b));
  };

  const createBranch = () => {
    const name = newBranchName.trim();
    if (name === '') {
      return;
    }
    setBranchMenu(false);
    setNewBranchMode(false);
    setNewBranchName('');
    void runGit((r) => gitCreateBranch(r, name));
  };

  const discardFile = (f: GitFileStatus) => {
    if (!window.confirm(`Discard changes in “${f.name}”? This can't be undone.`)) {
      return;
    }
    void runGit((r) => gitDiscard(r, [f.path], f.untracked));
  };

  const fileRow = (f: GitFileStatus) => (
    <div key={f.path} className={styles.fileRow}>
      <button
        type="button"
        className={styles.fileOpen}
        title={f.path}
        onClick={() =>
          f.isDir ? void revealInFileManager(f.absPath) : void openFileFromWorkspace(f.absPath)
        }
      >
        {f.isDir ? <FolderIcon /> : <FileIcon name={f.name} />}
        <span className={styles.fileName}>{f.name}</span>
        <span className={styles.dir}>{dirOf(f.path)}</span>
      </button>
      <button
        type="button"
        className={styles.action}
        title="Discard changes"
        aria-label={`Discard changes in ${f.name}`}
        disabled={busy}
        onClick={() => discardFile(f)}
      >
        ↺
      </button>
      <button
        type="button"
        className={styles.action}
        title={f.staged ? 'Unstage' : 'Stage'}
        aria-label={`${f.staged ? 'Unstage' : 'Stage'} ${f.name}`}
        disabled={busy}
        onClick={() =>
          void runGit((r) => (f.staged ? gitUnstage(r, [f.path]) : gitStage(r, [f.path])))
        }
      >
        {f.staged ? '−' : '+'}
      </button>
      <span
        className={`${styles.badge} ${f.untracked ? styles.badgeUntracked : ''}`}
        title={statusLabel(f)}
      >
        {statusBadge(f)}
      </span>
    </div>
  );

  return (
    <div className={styles.section}>
      <div className={styles.header}>
        <span className={styles.headerName}>Source Control</span>
        {status?.branch != null ? (
          <div className={styles.branchWrap}>
            <button
              type="button"
              className={styles.branch}
              title={`Branch: ${status.branch} — click to switch`}
              onClick={openBranchMenu}
            >
              {status.branch}
              {status.behind > 0 ? <span className={styles.track}>↓{status.behind}</span> : null}
              {status.ahead > 0 ? <span className={styles.track}>↑{status.ahead}</span> : null}
            </button>
            {branchMenu ? (
              <ul
                className={styles.branchMenu}
                role="menu"
                onMouseLeave={() => setBranchMenu(false)}
              >
                {newBranchMode ? (
                  <li role="none">
                    <input
                      className={styles.branchInput}
                      autoFocus
                      placeholder="New branch name"
                      value={newBranchName}
                      onChange={(e) => setNewBranchName(e.target.value)}
                      onKeyDown={(e) => {
                        if (e.key === 'Enter') {
                          createBranch();
                        } else if (e.key === 'Escape') {
                          setNewBranchMode(false);
                          setNewBranchName('');
                        }
                      }}
                    />
                  </li>
                ) : (
                  <li role="none">
                    <button
                      type="button"
                      role="menuitem"
                      className={styles.branchMenuItem}
                      onClick={() => setNewBranchMode(true)}
                    >
                      ＋ Create new branch…
                    </button>
                  </li>
                )}
                {branches.map((b) => (
                  <li role="none" key={b}>
                    <button
                      type="button"
                      role="menuitem"
                      className={`${styles.branchMenuItem} ${b === status.branch ? styles.branchCurrent : ''}`}
                      disabled={b === status.branch}
                      onClick={() => checkout(b)}
                    >
                      {b === status.branch ? '● ' : '  '}
                      {b}
                    </button>
                  </li>
                ))}
              </ul>
            ) : null}
          </div>
        ) : null}
        <div className={styles.headerActions}>
          {isRepo ? (
            <button
              type="button"
              className={styles.headerBtn}
              title="Sync (pull, then push)"
              aria-label="Sync"
              disabled={busy}
              onClick={() =>
                void runGit(async (r) => {
                  await gitPull(r);
                  await gitPush(r);
                })
              }
            >
              ⇅
            </button>
          ) : null}
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
          <p className={styles.errorLine} role="alert">
            {error}
          </p>
        ) : null}

        {status != null && !isRepo ? (
          <div className={styles.stateBox} role="status">
            <p className={styles.stateLine}>Not a git repository.</p>
            <button
              type="button"
              className={styles.commitBtn}
              disabled={busy}
              onClick={() => void runGit((r) => gitInit(r))}
            >
              Initialize Repository
            </button>
          </div>
        ) : (
          <>
            {isRepo ? (
              <div className={styles.commitBox}>
                <textarea
                  className={styles.commitMessage}
                  placeholder="Message — commits the staged changes"
                  rows={2}
                  value={message}
                  onChange={(e) => setMessage(e.target.value)}
                  onKeyDown={(e) => {
                    // Ctrl/Cmd+Enter commits, like VS Code.
                    if (
                      (e.ctrlKey || e.metaKey) &&
                      e.key === 'Enter' &&
                      staged.length > 0 &&
                      hasMessage
                    ) {
                      e.preventDefault();
                      doCommit();
                    }
                  }}
                />
                <div className={styles.commitRow}>
                  <button
                    type="button"
                    className={styles.commitBtn}
                    disabled={busy || staged.length === 0 || !hasMessage}
                    onClick={() => doCommit()}
                  >
                    ✓ Commit{staged.length > 0 ? ` (${staged.length})` : ''}
                  </button>
                  <button
                    type="button"
                    className={styles.commitCaret}
                    title="Commit options"
                    aria-label="Commit options"
                    aria-haspopup="menu"
                    disabled={busy}
                    onClick={() => setCommitMenu((v) => !v)}
                  >
                    ▾
                  </button>
                  {commitMenu ? (
                    <ul
                      className={styles.commitMenu}
                      role="menu"
                      onMouseLeave={() => setCommitMenu(false)}
                    >
                      <li role="none">
                        <button
                          type="button"
                          role="menuitem"
                          className={styles.commitMenuItem}
                          disabled={busy || staged.length === 0 || !hasMessage}
                          onClick={() => doCommit({ push: true })}
                        >
                          Commit &amp; Push
                        </button>
                      </li>
                      <li role="none">
                        <button
                          type="button"
                          role="menuitem"
                          className={styles.commitMenuItem}
                          disabled={busy || staged.length + changes.length === 0 || !hasMessage}
                          onClick={() => doCommit({ all: true })}
                        >
                          Commit All
                        </button>
                      </li>
                      <li role="none">
                        <button
                          type="button"
                          role="menuitem"
                          className={styles.commitMenuItem}
                          disabled={busy}
                          onClick={() => doCommit({ amend: true })}
                        >
                          Amend Last Commit
                        </button>
                      </li>
                    </ul>
                  ) : null}
                </div>
              </div>
            ) : null}

            {isRepo && staged.length === 0 && changes.length === 0 ? (
              <div className={styles.stateBox} role="status">
                <p className={styles.stateLine}>{busy ? 'Loading…' : 'No changes.'}</p>
              </div>
            ) : null}

            {staged.length > 0 ? (
              <>
                <div className={styles.groupLabel}>
                  <span>
                    Staged Changes <span className={styles.count}>{staged.length}</span>
                  </span>
                  <button
                    type="button"
                    className={styles.groupAction}
                    title="Unstage all"
                    aria-label="Unstage all"
                    disabled={busy}
                    onClick={() => void runGit((r) => gitUnstage(r, staged.map((f) => f.path)))}
                  >
                    −
                  </button>
                </div>
                {staged.map(fileRow)}
              </>
            ) : null}

            {changes.length > 0 ? (
              <>
                <div className={styles.groupLabel}>
                  <span>
                    Changes <span className={styles.count}>{changes.length}</span>
                  </span>
                  <button
                    type="button"
                    className={styles.groupAction}
                    title="Stage all"
                    aria-label="Stage all"
                    disabled={busy}
                    onClick={() => void runGit((r) => gitStage(r, changes.map((f) => f.path)))}
                  >
                    +
                  </button>
                </div>
                {changes.map(fileRow)}
              </>
            ) : null}
          </>
        )}
      </div>
    </div>
  );
}
