import { useCallback, useEffect, useId, useRef, useState, type KeyboardEvent } from 'react';
import { useNavigate } from 'react-router-dom';

import {
  createProjectIssueMessage,
  validateCreateProjectDraft,
  type CreateProjectValidationIssue,
} from '@domain/project/createProjectValidation';
import { DEFAULT_APP_PREFERENCES, withOpenedWorkspacePinned, type AppPreferences } from '@domain/preferences/appPreferences';
import { createProjectFolder, pickFolder } from '@infrastructure/tauri/bridge';
import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import { registerAgentCommands, registerAgentState } from '@shared/agent/agentBridge';

import styles from './WelcomeView.module.css';

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
      (loaded) => {
        if (!cancelled) {
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

  const handleOpenRecent = useCallback(async () => {
    const index = selectedIndexRef.current;
    if (index === null) {
      return;
    }
    await openPathAtIndex(index);
  }, [openPathAtIndex]);

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
      const newRoot = await createProjectFolder(parentPath.trim(), projectName.trim());
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
  }, [navigate, parentPath, prefs, projectName, showCreateIssue]);

  const handlePickParent = useCallback(async () => {
    const path = await pickFolder();
    if (path != null && path !== '') {
      setParentPath(path);
      setInfo(null);
    }
  }, []);

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
        <div className={styles.welcomeInner}>
          <h1 className={styles.title}>Create project</h1>
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
            />
            <label className={styles.fieldLabel} htmlFor="proj-lang">
              Language
            </label>
            <select
              id="proj-lang"
              className={styles.fieldInput}
              value={language}
              onChange={(e) => setLanguage(e.target.value)}
            >
              {LANGUAGE_OPTIONS.map((opt) => (
                <option key={opt} value={opt}>
                  {opt}
                </option>
              ))}
            </select>
            <span className={styles.fieldLabel}>Path</span>
            <div className={styles.pathRow}>
              <input className={styles.fieldInput} readOnly value={parentPath} placeholder="Choose parent folder…" />
              <button type="button" className={styles.actionBtn} onClick={() => void handlePickParent()}>
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
            <button type="button" className={styles.actionBtn} disabled={busy} onClick={() => void handleCreateProject()}>
              Create
            </button>
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
          </div>
        </div>
      </div>
    );
  }

  const activeOptionId = selectedIndex !== null ? `${listId}-opt-${selectedIndex}` : undefined;

  return (
    <div className={styles.welcomeRoot}>
      <div className={styles.welcomeInner}>
        <h1 className={styles.title}>Cremniy</h1>
        <p className={styles.subtitle}>Recent workspaces</p>
        <div
          className={styles.recentList}
          role="listbox"
          tabIndex={0}
          aria-label="Recent workspaces"
          aria-activedescendant={activeOptionId}
          onKeyDown={handleRecentListKeyDown}
        >
          {recentPaths.length === 0 ? (
            <div className={styles.emptyState}>No recent workspaces yet. Use Open… or Create.</div>
          ) : (
            recentPaths.map((path, index) => (
              <div
                key={path}
                id={`${listId}-opt-${index}`}
                role="option"
                aria-selected={selectedIndex === index}
                className={`${styles.listOption} ${selectedIndex === index ? styles.listOptionSelected : ''}`}
                onClick={() => setSelectedIndex(index)}
                onDoubleClick={() => void openPathAtIndex(index)}
              >
                {path}
              </div>
            ))
          )}
        </div>
        <div className={styles.buttonRow}>
          <button
            type="button"
            className={`${styles.actionBtn} ${selectedIndex !== null ? styles.actionBtnOpenReady : ''}`}
            disabled={selectedIndex === null}
            onClick={() => void handleOpenRecent()}
          >
            Open
          </button>
          <button type="button" className={styles.actionBtn} onClick={() => void handlePickFolder()}>
            Open...
          </button>
          <button
            type="button"
            className={styles.actionBtn}
            onClick={() => {
              resetCreateForm();
              setPage('create');
            }}
          >
            Create
          </button>
        </div>
      </div>
    </div>
  );
}
