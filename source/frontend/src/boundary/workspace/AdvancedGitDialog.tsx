import { useCallback, useEffect, useState } from 'react';

import {
  gitBranchDelete,
  gitBranchInfo,
  gitCheckout,
  gitCreateBranch,
  gitFetch,
  gitLog,
  gitMerge,
  gitMergeAbort,
  gitPublish,
  gitRebase,
  gitRebaseAbort,
  gitRebaseContinue,
  gitRemoteAdd,
  gitRemoteRemove,
  gitRemotes,
  gitRepos,
  gitSaveCredentials,
  gitStashApply,
  gitStashDrop,
  gitStashList,
  gitStashPop,
  gitStashPush,
  type GitBranchInfo,
  type GitCommit,
  type GitRemote,
  type GitRepoRef,
  type GitStashEntry,
} from '@infrastructure/tauri/bridge';

import styles from './AdvancedGitDialog.module.css';

export type AdvancedGitDialogProps = {
  open: boolean;
  onClose: () => void;
  workspaceRoot: string | null;
};

type Section = 'branches' | 'stash' | 'history' | 'remotes';

const SECTIONS: Array<{ id: Section; label: string }> = [
  { id: 'branches', label: 'Branches' },
  { id: 'stash', label: 'Stash' },
  { id: 'history', label: 'History' },
  { id: 'remotes', label: 'Remotes' },
];

function errText(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

type GitActionResult = { ok: boolean; error: string | null };

/** Per-section busy + error wiring: wraps an async op, flips `busy`, captures
 *  errors as an inline line, and runs an optional refresh on success. Returns
 *  the outcome so callers (merge/rebase) can route conflicts to their own UI. */
function useGitAction(): {
  busy: boolean;
  error: string | null;
  setError: (v: string | null) => void;
  run: (op: () => Promise<void>, after?: () => void | Promise<void>) => Promise<GitActionResult>;
} {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const run = useCallback(
    async (op: () => Promise<void>, after?: () => void | Promise<void>): Promise<GitActionResult> => {
      setBusy(true);
      setError(null);
      try {
        await op();
        if (after) await after();
        return { ok: true, error: null };
      } catch (e) {
        const message = errText(e);
        setError(message);
        return { ok: false, error: message };
      } finally {
        setBusy(false);
      }
    },
    [],
  );
  return { busy, error, setError, run };
}

// ── Branches ─────────────────────────────────────────────────────
function BranchesPane({ repo }: { repo: string }) {
  const { busy, error, setError, run } = useGitAction();
  const [info, setInfo] = useState<GitBranchInfo | null>(null);
  const [newName, setNewName] = useState('');
  const [target, setTarget] = useState<string | null>(null);
  // Conflict/abort surface: which operation left the repo mid-way.
  const [pending, setPending] = useState<'merge' | 'rebase' | null>(null);
  const [opError, setOpError] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    setInfo(await gitBranchInfo(repo));
  }, [repo]);

  useEffect(() => {
    setInfo(null);
    setTarget(null);
    setPending(null);
    setOpError(null);
    setError(null);
    void run(async () => {
      setInfo(await gitBranchInfo(repo));
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [repo]);

  // Merge/rebase report conflicts as an error — keep the message and reveal the
  // abort/continue controls instead of surfacing it as a plain section error.
  const runMergeRebase = useCallback(
    async (kind: 'merge' | 'rebase', branch: string) => {
      setOpError(null);
      const res = await run(
        () => (kind === 'merge' ? gitMerge(repo, branch) : gitRebase(repo, branch)),
        refresh,
      );
      if (res.ok) {
        setPending(null);
      } else {
        setPending(kind);
        setOpError(res.error);
      }
    },
    [refresh, repo, run],
  );

  const current = info?.current ?? null;
  const local = info?.local ?? [];
  const remote = info?.remote ?? [];

  return (
    <>
      <h2 className={styles.catTitle}>Branches</h2>

      <div className={styles.currentBranch}>
        <span className={styles.currentBranchLabel}>Current</span>
        <span className={styles.currentBranchName}>{current ?? '—'}</span>
      </div>

      <div className={styles.formRow}>
        <input
          className={`${styles.input} ${styles.inputGrow}`}
          type="text"
          aria-label="New branch name"
          placeholder="New branch name"
          value={newName}
          onChange={(e) => setNewName(e.target.value)}
          disabled={busy}
        />
        <button
          type="button"
          className={styles.btn}
          disabled={busy || newName.trim() === ''}
          onClick={() =>
            void run(() => gitCreateBranch(repo, newName.trim()), async () => {
              setNewName('');
              await refresh();
            })
          }
        >
          Create branch
        </button>
        <button type="button" className={styles.btn} disabled={busy} onClick={() => void run(() => gitFetch(repo), refresh)}>
          Fetch
        </button>
      </div>

      <div className={styles.groupTitle}>Local</div>
      {local.length === 0 ? (
        <p className={styles.emptyLine}>No local branches.</p>
      ) : (
        <ul className={styles.list}>
          {local.map((name) => {
            const isCurrent = name === current;
            return (
              <li key={name} className={styles.row}>
                <label className={styles.pick}>
                  <input
                    type="radio"
                    name="branch-target"
                    checked={target === name}
                    onChange={() => setTarget(name)}
                    disabled={busy}
                  />
                  <span className={`${styles.rowName} ${styles.mono}`}>{name}</span>
                </label>
                {isCurrent ? <span className={styles.tagCurrent}>current</span> : null}
                <div className={styles.rowActions}>
                  <button
                    type="button"
                    className={styles.btn}
                    disabled={busy || isCurrent}
                    onClick={() => void run(() => gitCheckout(repo, name), refresh)}
                  >
                    Checkout
                  </button>
                  <button
                    type="button"
                    className={styles.btn}
                    disabled={busy || isCurrent}
                    onClick={() => void run(() => gitBranchDelete(repo, name, false), refresh)}
                  >
                    Delete
                  </button>
                  <button
                    type="button"
                    className={`${styles.btn} ${styles.btnDanger}`}
                    disabled={busy || isCurrent}
                    title="Delete even if not fully merged"
                    onClick={() => void run(() => gitBranchDelete(repo, name, true), refresh)}
                  >
                    Force
                  </button>
                </div>
              </li>
            );
          })}
        </ul>
      )}

      <div className={styles.groupTitle}>Remote</div>
      {remote.length === 0 ? (
        <p className={styles.emptyLine}>No remote branches.</p>
      ) : (
        <ul className={styles.list}>
          {remote.map((name) => (
            <li key={name} className={styles.row}>
              <span className={`${styles.rowName} ${styles.mono}`}>{name}</span>
            </li>
          ))}
        </ul>
      )}

      <div className={styles.groupTitle}>Integrate selected branch</div>
      <p className={styles.helpText}>
        {target != null
          ? `Selected: ${target}. Merge brings it into ${current ?? 'current'}; rebase replays ${current ?? 'current'} onto it.`
          : 'Pick a local branch above, then merge it into the current branch or rebase the current branch onto it.'}
      </p>
      <div className={styles.formRow} style={{ marginTop: '0.5rem' }}>
        <button
          type="button"
          className={styles.btn}
          disabled={busy || target == null || target === current}
          onClick={() => {
            if (target != null) void runMergeRebase('merge', target);
          }}
        >
          Merge into current
        </button>
        <button
          type="button"
          className={styles.btn}
          disabled={busy || target == null || target === current}
          onClick={() => {
            if (target != null) void runMergeRebase('rebase', target);
          }}
        >
          Rebase onto selected
        </button>
      </div>

      {pending != null ? (
        <div className={styles.conflictBox} role="alert">
          <p className={styles.conflictText}>{opError ?? error ?? 'Operation could not complete.'}</p>
          <div className={styles.conflictActions}>
            {pending === 'merge' ? (
              <button
                type="button"
                className={styles.btn}
                disabled={busy}
                onClick={() =>
                  void run(() => gitMergeAbort(repo), async () => {
                    setPending(null);
                    setOpError(null);
                    await refresh();
                  })
                }
              >
                Abort merge
              </button>
            ) : (
              <>
                <button
                  type="button"
                  className={styles.btn}
                  disabled={busy}
                  onClick={() =>
                    void run(() => gitRebaseContinue(repo), async () => {
                      setPending(null);
                      setOpError(null);
                      await refresh();
                    })
                  }
                >
                  Continue rebase
                </button>
                <button
                  type="button"
                  className={styles.btn}
                  disabled={busy}
                  onClick={() =>
                    void run(() => gitRebaseAbort(repo), async () => {
                      setPending(null);
                      setOpError(null);
                      await refresh();
                    })
                  }
                >
                  Abort rebase
                </button>
              </>
            )}
          </div>
        </div>
      ) : null}

      {/* Non-integrate failures (checkout, delete, fetch…) show inline; merge /
          rebase conflicts surface in the conflict box above instead. */}
      {pending == null && error != null ? <p className={styles.error}>{error}</p> : null}
    </>
  );
}

// ── Stash ────────────────────────────────────────────────────────
function StashPane({ repo }: { repo: string }) {
  const { busy, error, setError, run } = useGitAction();
  const [entries, setEntries] = useState<GitStashEntry[]>([]);
  const [message, setMessage] = useState('');

  const refresh = useCallback(async () => {
    setEntries(await gitStashList(repo));
  }, [repo]);

  useEffect(() => {
    setEntries([]);
    setError(null);
    void run(async () => {
      setEntries(await gitStashList(repo));
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [repo]);

  return (
    <>
      <h2 className={styles.catTitle}>Stash</h2>

      <div className={styles.formRow}>
        <input
          className={`${styles.input} ${styles.inputGrow}`}
          type="text"
          aria-label="Stash message"
          placeholder="Optional message"
          value={message}
          onChange={(e) => setMessage(e.target.value)}
          disabled={busy}
        />
        <button
          type="button"
          className={styles.btn}
          disabled={busy}
          onClick={() =>
            void run(
              () => gitStashPush(repo, message.trim() === '' ? undefined : message.trim()),
              async () => {
                setMessage('');
                await refresh();
              },
            )
          }
        >
          Stash changes
        </button>
      </div>

      <div className={styles.groupTitle}>Stashes</div>
      {entries.length === 0 ? (
        <p className={styles.emptyLine}>No stashes.</p>
      ) : (
        <ul className={styles.list}>
          {entries.map((s) => (
            <li key={s.index} className={styles.row}>
              <div className={styles.rowMain}>
                <span className={styles.rowName}>{s.message}</span>
                <span className={styles.rowSub}>stash@{'{'}{s.index}{'}'}</span>
              </div>
              <div className={styles.rowActions}>
                <button
                  type="button"
                  className={styles.btn}
                  disabled={busy}
                  onClick={() => void run(() => gitStashApply(repo, s.index), refresh)}
                >
                  Apply
                </button>
                <button
                  type="button"
                  className={styles.btn}
                  disabled={busy}
                  onClick={() => void run(() => gitStashPop(repo, s.index), refresh)}
                >
                  Pop
                </button>
                <button
                  type="button"
                  className={`${styles.btn} ${styles.btnDanger}`}
                  disabled={busy}
                  onClick={() => void run(() => gitStashDrop(repo, s.index), refresh)}
                >
                  Drop
                </button>
              </div>
            </li>
          ))}
        </ul>
      )}

      {error != null ? <p className={styles.error}>{error}</p> : null}
    </>
  );
}

// ── History ──────────────────────────────────────────────────────
function HistoryPane({ repo }: { repo: string }) {
  const { busy, error, setError, run } = useGitAction();
  const [commits, setCommits] = useState<GitCommit[]>([]);

  useEffect(() => {
    setCommits([]);
    setError(null);
    void run(async () => {
      setCommits(await gitLog(repo, 50));
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [repo]);

  return (
    <>
      <h2 className={styles.catTitle}>History</h2>
      {commits.length === 0 ? (
        <p className={styles.emptyLine}>{busy ? 'Loading…' : 'No commits.'}</p>
      ) : (
        <ul className={styles.list}>
          {commits.map((c) => (
            <li key={c.hash} className={styles.commitRow}>
              <span className={styles.commitHash}>{c.shortHash}</span>
              <div className={styles.commitBody}>
                <span className={styles.commitSubject}>{c.subject}</span>
                <span className={styles.commitMeta}>
                  {c.author} · {c.date}
                </span>
              </div>
            </li>
          ))}
        </ul>
      )}
      {error != null ? <p className={styles.error}>{error}</p> : null}
    </>
  );
}

// ── Remotes ──────────────────────────────────────────────────────
function RemotesPane({ repo, branchInfo }: { repo: string; branchInfo: GitBranchInfo | null }) {
  const { busy, error, setError, run } = useGitAction();
  const [remotes, setRemotes] = useState<GitRemote[]>([]);
  const [name, setName] = useState('');
  const [url, setUrl] = useState('');
  // Credentials form.
  const [credUrl, setCredUrl] = useState('');
  const [credUser, setCredUser] = useState('');
  const [credToken, setCredToken] = useState('');
  const [credSaved, setCredSaved] = useState(false);
  // Publish form.
  const [pubRemote, setPubRemote] = useState('');
  const [pubBranch, setPubBranch] = useState('');
  const [published, setPublished] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    const list = await gitRemotes(repo);
    setRemotes(list);
    setPubRemote((prev) => (prev !== '' ? prev : list[0]?.name ?? ''));
  }, [repo]);

  useEffect(() => {
    setRemotes([]);
    setError(null);
    void run(async () => {
      await refresh();
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [repo]);

  // Default the publish branch to the current one once branch info arrives.
  useEffect(() => {
    if (branchInfo?.current != null && pubBranch === '') {
      setPubBranch(branchInfo.current);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [branchInfo?.current]);

  return (
    <>
      <h2 className={styles.catTitle}>Remotes</h2>

      <div className={styles.groupTitle + ' ' + styles.groupTitleFirst}>Configured</div>
      {remotes.length === 0 ? (
        <p className={styles.emptyLine}>No remotes.</p>
      ) : (
        <ul className={styles.list}>
          {remotes.map((r) => (
            <li key={r.name} className={styles.row}>
              <div className={styles.rowMain}>
                <span className={`${styles.rowName} ${styles.mono}`}>{r.name}</span>
                <span className={styles.rowSub}>{r.url}</span>
              </div>
              <div className={styles.rowActions}>
                <button
                  type="button"
                  className={`${styles.btn} ${styles.btnDanger}`}
                  disabled={busy}
                  onClick={() => void run(() => gitRemoteRemove(repo, r.name), refresh)}
                >
                  Remove
                </button>
              </div>
            </li>
          ))}
        </ul>
      )}

      <div className={styles.groupTitle}>Add remote</div>
      <div className={styles.formRow}>
        <input
          className={styles.input}
          type="text"
          aria-label="Remote name"
          placeholder="name (e.g. origin)"
          value={name}
          onChange={(e) => setName(e.target.value)}
          disabled={busy}
        />
        <input
          className={`${styles.input} ${styles.inputGrow}`}
          type="text"
          aria-label="Remote URL"
          placeholder="https://… or git@…"
          value={url}
          onChange={(e) => setUrl(e.target.value)}
          disabled={busy}
        />
        <button
          type="button"
          className={styles.btn}
          disabled={busy || name.trim() === '' || url.trim() === ''}
          onClick={() =>
            void run(() => gitRemoteAdd(repo, name.trim(), url.trim()), async () => {
              setName('');
              setUrl('');
              await refresh();
            })
          }
        >
          Add remote
        </button>
      </div>

      <div className={styles.groupTitle}>Publish branch</div>
      <div className={styles.formRow}>
        <select
          className={styles.select}
          aria-label="Publish remote"
          value={pubRemote}
          onChange={(e) => setPubRemote(e.target.value)}
          disabled={busy || remotes.length === 0}
        >
          {remotes.length === 0 ? <option value="">No remotes</option> : null}
          {remotes.map((r) => (
            <option key={r.name} value={r.name}>
              {r.name}
            </option>
          ))}
        </select>
        <input
          className={`${styles.input} ${styles.inputGrow}`}
          type="text"
          aria-label="Publish branch"
          placeholder="branch"
          value={pubBranch}
          onChange={(e) => setPubBranch(e.target.value)}
          disabled={busy}
        />
        <button
          type="button"
          className={styles.btnPrimary}
          disabled={busy || pubRemote === '' || pubBranch.trim() === ''}
          onClick={() =>
            void run(
              () => gitPublish(repo, pubRemote, pubBranch.trim()),
              () => setPublished(`Published ${pubBranch.trim()} to ${pubRemote}.`),
            )
          }
        >
          Publish
        </button>
      </div>
      {published != null ? <p className={styles.helpText}>{published}</p> : null}

      <div className={styles.groupTitle}>Credentials</div>
      <p className={styles.helpText}>
        Stored in the OS credential manager — never written to repo config or the remote URL. SSH
        remotes use your keys and need no token here.
      </p>
      <div className={styles.formStack} style={{ marginTop: '0.5rem' }}>
        <input
          className={styles.fullInput}
          type="text"
          aria-label="Credential URL"
          placeholder="Host or remote URL (https://github.com)"
          value={credUrl}
          onChange={(e) => setCredUrl(e.target.value)}
          disabled={busy}
        />
        <div className={styles.formRow}>
          <input
            className={`${styles.input} ${styles.inputGrow}`}
            type="text"
            aria-label="Credential username"
            placeholder="username"
            value={credUser}
            onChange={(e) => setCredUser(e.target.value)}
            disabled={busy}
          />
          <input
            className={`${styles.input} ${styles.inputGrow}`}
            type="password"
            aria-label="Credential token"
            placeholder="token / password"
            value={credToken}
            onChange={(e) => setCredToken(e.target.value)}
            disabled={busy}
          />
          <button
            type="button"
            className={styles.btn}
            disabled={busy || credUrl.trim() === '' || credUser.trim() === '' || credToken === ''}
            onClick={() =>
              void run(
                () => gitSaveCredentials(credUrl.trim(), credUser.trim(), credToken),
                () => {
                  setCredToken('');
                  setCredSaved(true);
                },
              )
            }
          >
            Save credentials
          </button>
        </div>
      </div>
      {credSaved ? <p className={styles.helpText}>Credentials saved.</p> : null}

      {error != null ? <p className={styles.error}>{error}</p> : null}
    </>
  );
}

export function AdvancedGitDialog({ open, onClose, workspaceRoot }: AdvancedGitDialogProps) {
  const [section, setSection] = useState<Section>('branches');
  const [repos, setRepos] = useState<GitRepoRef[] | null>(null);
  const [repo, setRepo] = useState<string | null>(null);
  const [reposError, setReposError] = useState<string | null>(null);
  // Shared branch info so Remotes can default the publish branch sensibly.
  const [branchInfo, setBranchInfo] = useState<GitBranchInfo | null>(null);

  // Discover repos whenever the dialog opens (or the workspace changes).
  useEffect(() => {
    if (!open) {
      return;
    }
    let cancelled = false;
    setReposError(null);
    setRepos(null);
    if (workspaceRoot == null || workspaceRoot === '') {
      setRepos([]);
      setRepo(null);
      return;
    }
    void gitRepos(workspaceRoot).then(
      (list) => {
        if (cancelled) return;
        setRepos(list);
        setRepo(list[0]?.path ?? null);
      },
      (e) => {
        if (cancelled) return;
        setReposError(errText(e));
        setRepos([]);
        setRepo(null);
      },
    );
    return () => {
      cancelled = true;
    };
  }, [open, workspaceRoot]);

  // Keep shared branch info in step with the selected repo so the Remotes pane's
  // publish branch defaults correctly even before that pane is opened.
  useEffect(() => {
    if (!open || repo == null) {
      setBranchInfo(null);
      return;
    }
    let cancelled = false;
    void gitBranchInfo(repo).then(
      (i) => {
        if (!cancelled) setBranchInfo(i);
      },
      () => {
        if (!cancelled) setBranchInfo(null);
      },
    );
    return () => {
      cancelled = true;
    };
  }, [open, repo, section]);

  if (!open) {
    return null;
  }

  const hasRepo = repo != null;

  return (
    <div
      className={styles.backdrop}
      role="presentation"
      onMouseDown={(e) => e.target === e.currentTarget && onClose()}
    >
      <div
        className={styles.dialog}
        role="dialog"
        aria-modal
        aria-labelledby="advanced-git-dialog-title"
        onMouseDown={(e) => e.stopPropagation()}
        onKeyDown={(e) => {
          if (e.key === 'Escape') {
            onClose();
          }
        }}
      >
        <nav className={styles.sidebar} aria-label="Advanced Git sections">
          <span className={styles.sidebarTitle} id="advanced-git-dialog-title">
            Advanced Git
          </span>
          {SECTIONS.map((s) => (
            <button
              key={s.id}
              type="button"
              className={`${styles.navItem} ${section === s.id ? styles.navItemActive : ''}`}
              aria-current={section === s.id}
              disabled={!hasRepo}
              onClick={() => setSection(s.id)}
            >
              {s.label}
            </button>
          ))}
        </nav>

        <div className={styles.main}>
          {repos != null && repos.length > 0 ? (
            <div className={styles.repoBar}>
              <span className={styles.repoBarLabel}>Repository</span>
              <select
                className={styles.select}
                aria-label="Repository"
                value={repo ?? ''}
                onChange={(e) => setRepo(e.target.value)}
              >
                {repos.map((r) => (
                  <option key={r.path} value={r.path}>
                    {r.name}
                  </option>
                ))}
              </select>
            </div>
          ) : null}

          <div className={styles.mainScroll}>
            {repos == null ? (
              <p className={styles.emptyLine}>Scanning for repositories…</p>
            ) : repo == null ? (
              <div className={styles.emptyState} role="status">
                {reposError != null ? (
                  <p className={styles.error}>{reposError}</p>
                ) : workspaceRoot == null || workspaceRoot === '' ? (
                  <p>Open a workspace folder to manage its git repositories.</p>
                ) : (
                  <p>No git repository found in this workspace.</p>
                )}
              </div>
            ) : section === 'branches' ? (
              <BranchesPane key={repo} repo={repo} />
            ) : section === 'stash' ? (
              <StashPane key={repo} repo={repo} />
            ) : section === 'history' ? (
              <HistoryPane key={repo} repo={repo} />
            ) : (
              <RemotesPane key={repo} repo={repo} branchInfo={branchInfo} />
            )}
          </div>

          <div className={styles.footer}>
            <span className={styles.footerSpacer} />
            <button type="button" className={styles.btn} onClick={onClose}>
              Close
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
