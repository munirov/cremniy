import { useCallback, useEffect, useId, useRef, useState, type KeyboardEvent } from 'react';
import { useNavigate } from 'react-router-dom';

import {
  createProjectIssueMessage,
  validateCreateProjectDraft,
  type CreateProjectValidationIssue,
} from '@domain/project/createProjectValidation';
import { DEFAULT_APP_PREFERENCES, withOpenedWorkspacePinned, type AppPreferences } from '@domain/preferences/appPreferences';
import {
  createCremniyProject,
  pickFile,
  pickFolder,
} from '@infrastructure/tauri/bridge';
import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import { registerAgentCommands, registerAgentState } from '@shared/agent/agentBridge';
import { Select } from '@boundary/common/Select';
import {
  emptyCremniyMeta,
  stringifyCremniyMeta,
  type CremniyLanguage,
} from '@domain/project/cremniyMeta';

import styles from './WelcomeView.module.css';

/**
 * Strip Windows extended-length / UNC prefixes (`\\?\`, `\\?\UNC\`) before
 * showing a path. Tauri's canonicalize() returns the long form even for plain
 * drive paths; we don't want that leaking into the UI.
 */
function prettify(path: string): string {
  return path.replace(/^\\\\\?\\(UNC\\)?/, '').replace(/^\/\/\?\/(UNC\/)?/, '');
}

function fileNameOf(path: string): string {
  const p = prettify(path);
  const m = p.match(/[^\\/]+$/);
  return m ? m[0] : p;
}

function parentOf(path: string): string {
  const p = prettify(path);
  const m = p.match(/^(.*)[\\/][^\\/]+$/);
  return m ? m[1] : '';
}

type WelcomePage = 'welcome' | 'create';

const LANGUAGE_OPTIONS = ['C', 'C++', 'ASM', 'C + ASM', 'Custom'] as const;

export function WelcomeView() {
  const navigate = useNavigate();
  const listId = useId();
  const selectedIndexRef = useRef<number | null>(null);

  const [page, setPage] = useState<WelcomePage>('welcome');
  const [selectedIndex, setSelectedIndex] = useState<number | null>(null);
  const [prefs, setPrefs] = useState<AppPreferences | null>(null);

  const [projectName, setProjectName] = useState('');
  const [language, setLanguage] = useState<string>(LANGUAGE_OPTIONS[0]);
  const [parentPath, setParentPath] = useState('');
  const [info, setInfo] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);

  selectedIndexRef.current = selectedIndex;

  useEffect(() => {
    let cancelled = false;
    void loadPreferences().then(
      async (loaded) => {
        if (cancelled) return;
        // Drop recent paths whose folder no longer exists (Qt parity:
        // ProjectsHistoryManager.checkDirectoryExists). list_directory throws
        // when the path is gone, so a successful call means the folder is
        // still reachable.
        const checks = await Promise.all(
          loaded.recentWorkspacePaths.map(async (path) => {
            try {
              const { listDirectoryEntries } = await import('@infrastructure/tauri/bridge');
              await listDirectoryEntries(path, path);
              return path;
            } catch {
              return null;
            }
          }),
        );
        const survivors = checks.filter((p): p is string => p != null);
        if (cancelled) return;
        if (survivors.length !== loaded.recentWorkspacePaths.length) {
          const pruned = { ...loaded, recentWorkspacePaths: survivors };
          setPrefs(pruned);
          void savePreferences(pruned).catch(() => undefined);
        } else {
          setPrefs(loaded);
        }
      },
      () => {
        if (!cancelled) {
          setPrefs({ ...DEFAULT_APP_PREFERENCES });
        }
      },
    );
    return () => {
      cancelled = true;
    };
  }, []);

  const recentPaths = prefs?.recentWorkspacePaths ?? [];

  useEffect(() => {
    if (selectedIndex !== null && selectedIndex >= recentPaths.length) {
      setSelectedIndex(null);
    }
  }, [recentPaths.length, selectedIndex]);

  const resetCreateForm = useCallback(() => {
    setProjectName('');
    setLanguage(LANGUAGE_OPTIONS[0]);
    setParentPath('');
    setInfo(null);
  }, []);

  const openPathAtIndex = useCallback(
    async (index: number) => {
      const path = recentPaths[index];
      if (path == null || path === '') {
        return;
      }
      const base = prefs ?? DEFAULT_APP_PREFERENCES;
      const next = withOpenedWorkspacePinned(base, path);
      await savePreferences(next);
      setPrefs(next);
      navigate(`/ide?root=${encodeURIComponent(path)}`);
    },
    [navigate, prefs, recentPaths],
  );

  const handlePickFolder = useCallback(async () => {
    const path = await pickFolder();
    if (path == null || path === '') {
      return;
    }
    const base = prefs ?? DEFAULT_APP_PREFERENCES;
    const next = withOpenedWorkspacePinned(base, path);
    await savePreferences(next);
    setPrefs(next);
    navigate(`/ide?root=${encodeURIComponent(path)}`);
  }, [navigate, prefs]);

  const showCreateIssue = useCallback((issue: CreateProjectValidationIssue) => {
    setInfo(createProjectIssueMessage(issue));
  }, []);

  const handleCreateProject = useCallback(async () => {
    setInfo(null);
    const draftIssue = validateCreateProjectDraft({ projectName, parentDirectoryPath: parentPath });
    if (draftIssue != null) {
      showCreateIssue(draftIssue);
      return;
    }
    setBusy(true);
    try {
      const trimmedName = projectName.trim();
      const meta = emptyCremniyMeta(trimmedName, language as CremniyLanguage);
      const newRoot = await createCremniyProject(
        parentPath.trim(),
        trimmedName,
        stringifyCremniyMeta(meta),
      );
      const base = prefs ?? DEFAULT_APP_PREFERENCES;
      const next = withOpenedWorkspacePinned(base, newRoot);
      await savePreferences(next);
      setPrefs(next);
      navigate(`/ide?root=${encodeURIComponent(newRoot)}`);
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      if (/already exists|destination already exists/i.test(msg)) {
        showCreateIssue({ field: 'general', code: 'targetExists' });
      } else if (/not a directory|parent_path/i.test(msg)) {
        showCreateIssue({ field: 'path', code: 'pathNotDirectory' });
      } else {
        showCreateIssue({ field: 'general', code: 'createFailed' });
      }
    } finally {
      setBusy(false);
    }
  }, [language, navigate, parentPath, prefs, projectName, showCreateIssue]);

  const handlePickParent = useCallback(async () => {
    const path = await pickFolder();
    if (path != null && path !== '') {
      setParentPath(path);
      setInfo(null);
    }
  }, []);

  const handlePickFile = useCallback(async () => {
    const path = await pickFile();
    if (path == null || path === '') return;
    const parent = parentOf(path);
    if (parent === '') return;
    const base = prefs ?? DEFAULT_APP_PREFERENCES;
    const next = withOpenedWorkspacePinned(base, parent);
    await savePreferences(next);
    setPrefs(next);
    navigate(`/ide?root=${encodeURIComponent(parent)}`);
  }, [navigate, prefs]);

  const handleRecentListKeyDown = useCallback(
    (e: KeyboardEvent<HTMLDivElement>) => {
      if (recentPaths.length === 0) {
        return;
      }
      switch (e.key) {
        case 'ArrowDown': {
          e.preventDefault();
          setSelectedIndex((prev) => {
            if (prev === null) {
              return 0;
            }
            return Math.min(prev + 1, recentPaths.length - 1);
          });
          break;
        }
        case 'ArrowUp': {
          e.preventDefault();
          setSelectedIndex((prev) => {
            if (prev === null) {
              return recentPaths.length - 1;
            }
            return Math.max(prev - 1, 0);
          });
          break;
        }
        case 'Home': {
          e.preventDefault();
          setSelectedIndex(0);
          break;
        }
        case 'End': {
          e.preventDefault();
          setSelectedIndex(recentPaths.length - 1);
          break;
        }
        case 'Enter': {
          const idx = selectedIndexRef.current;
          if (idx !== null) {
            e.preventDefault();
            void openPathAtIndex(idx);
          }
          break;
        }
        default:
          break;
      }
    },
    [openPathAtIndex, recentPaths.length],
  );

  // welcome.* commands + `ui` state for window.cremniy.
  // Docs: documentation/architecture/AGENT_CONTROL.md
  const agentWelcomeRef = useRef({ page, recentPaths, openPathAtIndex, handlePickFolder, setPage });
  useEffect(() => {
    agentWelcomeRef.current = { page, recentPaths, openPathAtIndex, handlePickFolder, setPage };
  });
  useEffect(() => {
    const unregisterState = registerAgentState('ui', () => ({
      route: 'welcome',
      page: agentWelcomeRef.current.page,
      recentWorkspacePaths: agentWelcomeRef.current.recentPaths,
    }));
    const unregisterCommands = registerAgentCommands([
      {
        name: 'welcome.openRecent',
        description: 'Open a recent workspace by { index } into the recent list.',
        run: (args) => {
          const index = Number(args.index);
          if (!Number.isInteger(index)) {
            throw new Error('welcome.openRecent requires an integer { index }.');
          }
          return agentWelcomeRef.current.openPathAtIndex(index);
        },
      },
      {
        name: 'welcome.openFolder',
        description: 'Open the native folder picker and enter the chosen workspace.',
        run: () => agentWelcomeRef.current.handlePickFolder(),
      },
      {
        name: 'welcome.gotoCreate',
        description: 'Switch the Welcome screen to the Create-project form.',
        run: () => agentWelcomeRef.current.setPage('create'),
      },
    ]);
    return () => {
      unregisterState();
      unregisterCommands();
    };
  }, []);

  if (page === 'create') {
    return (
      <div className={styles.welcomeRoot}>
        <div className={styles.welcomeCenter}>
          <div className={styles.hero}>
            <img
              src="/cremniy-logo.svg"
              alt=""
              aria-hidden
              className={styles.heroLogo}
              draggable={false}
            />
            <div className={styles.heroName}>NEW PROJECT</div>
          </div>

          <div className={styles.createGrid}>
            <label className={styles.fieldLabel} htmlFor="proj-name">
              Project name
            </label>
            <input
              id="proj-name"
              className={styles.fieldInput}
              value={projectName}
              onChange={(e) => {
                setProjectName(e.target.value);
                setInfo(null);
              }}
              autoComplete="off"
              spellCheck={false}
              placeholder="my-project"
            />

            <label className={styles.fieldLabel} htmlFor="proj-lang">
              Language
            </label>
            <Select<(typeof LANGUAGE_OPTIONS)[number]>
              id="proj-lang"
              value={language as (typeof LANGUAGE_OPTIONS)[number]}
              options={LANGUAGE_OPTIONS.map((opt) => ({ value: opt, label: opt }))}
              onChange={(v) => setLanguage(v)}
              ariaLabel="Project language"
            />

            <span className={styles.fieldLabel}>Path</span>
            <div className={styles.pathRow}>
              <input
                className={styles.fieldInput}
                readOnly
                value={parentPath}
                placeholder="Choose parent folder…"
              />
              <button
                type="button"
                className={styles.actionBtn}
                onClick={() => void handlePickParent()}
              >
                Browse…
              </button>
            </div>
          </div>

          {info != null ? (
            <p className={styles.infoLabel} role="alert">
              {info}
            </p>
          ) : null}

          <div className={styles.buttonRow}>
            <button
              type="button"
              className={styles.actionBtn}
              disabled={busy}
              onClick={() => {
                resetCreateForm();
                setPage('welcome');
              }}
            >
              Back
            </button>
            <button
              type="button"
              className={`${styles.actionBtn} ${styles.actionBtnPrimary}`}
              disabled={busy}
              onClick={() => void handleCreateProject()}
            >
              Create
            </button>
          </div>
        </div>
      </div>
    );
  }

  const activeOptionId = selectedIndex !== null ? `${listId}-opt-${selectedIndex}` : undefined;
  // Recent list is capped at 5 visible rows like Cursor; the rest live behind
  // the "View all" link (which currently jumps to the same list — Recent
  // window is a follow-up).
  const visibleRecents = recentPaths.slice(0, 5);
  const overflowCount = recentPaths.length;

  return (
    <div className={styles.welcomeRoot}>
      <div className={styles.welcomeCenter}>
        <div className={styles.hero}>
          <img src="/cremniy-logo.svg" alt="" aria-hidden className={styles.heroLogo} draggable={false} />
          <div className={styles.heroName}>CREMNIY</div>
          <div className={styles.heroPlan}>Low-level IDE</div>
        </div>

        <div className={styles.cardGrid}>
          <button type="button" className={styles.card} onClick={() => void handlePickFolder()}>
            <svg className={styles.cardIcon} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" aria-hidden>
              <path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z" />
            </svg>
            <span className={styles.cardLabel}>Open project</span>
          </button>
          <button
            type="button"
            className={styles.card}
            onClick={() => {
              resetCreateForm();
              setPage('create');
            }}
          >
            <svg className={styles.cardIcon} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" aria-hidden>
              <path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z" />
              <path d="M12 10v6M9 13h6" />
            </svg>
            <span className={styles.cardLabel}>New project</span>
          </button>
          <button type="button" className={styles.card} onClick={() => void handlePickFile()}>
            <svg className={styles.cardIcon} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round" aria-hidden>
              <path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z" />
              <path d="M14 3v5h5" />
            </svg>
            <span className={styles.cardLabel}>Open file</span>
          </button>
        </div>

        <section className={styles.recentSection} aria-label="Recent projects">
          <header className={styles.recentHeader}>
            <span>Recent projects</span>
            {overflowCount > 0 ? (
              <span className={styles.recentViewAll}>View all ({overflowCount})</span>
            ) : null}
          </header>
          <div
            className={styles.recentList}
            role="listbox"
            tabIndex={0}
            aria-label="Recent workspaces"
            aria-activedescendant={activeOptionId}
            onKeyDown={handleRecentListKeyDown}
          >
            {visibleRecents.length === 0 ? (
              <div className={styles.emptyState}>No recent projects yet.</div>
            ) : (
              visibleRecents.map((path, index) => (
                <div
                  key={path}
                  id={`${listId}-opt-${index}`}
                  role="option"
                  aria-selected={selectedIndex === index}
                  className={`${styles.recentRow} ${selectedIndex === index ? styles.recentRowSelected : ''}`}
                  onClick={() => setSelectedIndex(index)}
                  onDoubleClick={() => void openPathAtIndex(index)}
                  title={path}
                >
                  <span className={styles.recentName}>{fileNameOf(path)}</span>
                  <span className={styles.recentPath}>{parentOf(path) || path}</span>
                </div>
              ))
            )}
          </div>
        </section>
      </div>
    </div>
  );
}
