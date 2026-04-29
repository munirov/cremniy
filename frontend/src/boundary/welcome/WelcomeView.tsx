import { useCallback, useEffect, useState } from 'react';
import { Link, useNavigate } from 'react-router-dom';

import { DEFAULT_APP_PREFERENCES, withOpenedWorkspacePinned, type AppPreferences } from '@domain/preferences/appPreferences';
import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import { pickFolder } from '@infrastructure/tauri/bridge';

import styles from './WelcomeView.module.css';

/** Mock paths only — no filesystem access (parity with Qt QListView + history). */
export const DEMO_RECENT_WORKSPACE_PATHS = [
  'C:/Users/demo/Documents/project-alpha',
  'D:/work/cremniy-sample',
  'C:/Projects/stm32-template',
] as const;

export function WelcomeView() {
  const navigate = useNavigate();
  const [selectedIndex, setSelectedIndex] = useState<number | null>(null);
  const [prefs, setPrefs] = useState<AppPreferences | null>(null);

  useEffect(() => {
    let cancelled = false;
    void loadPreferences().then(
      (loaded) => {
        if (!cancelled) {
          const recents =
            loaded.recentWorkspacePaths.length > 0
              ? loaded.recentWorkspacePaths
              : [...DEMO_RECENT_WORKSPACE_PATHS];
          setPrefs({ ...loaded, recentWorkspacePaths: recents });
        }
      },
      () => {
        if (!cancelled) {
          setPrefs({
            ...DEFAULT_APP_PREFERENCES,
            recentWorkspacePaths: [...DEMO_RECENT_WORKSPACE_PATHS],
          });
        }
      },
    );
    return () => {
      cancelled = true;
    };
  }, []);

  const recentPaths = prefs?.recentWorkspacePaths ?? [];

  const handleOpenRecent = useCallback(() => {
    if (selectedIndex === null) return;
  }, [selectedIndex]);

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

  return (
    <div className={styles.welcomeRoot}>
      <div className={styles.welcomeInner}>
        <h1 className={styles.title}>Cremniy</h1>
        <ul className={styles.recentList} aria-label="Recent projects">
          {recentPaths.map((path, index) => (
            <li key={path} className={styles.listRow}>
              <button
                type="button"
                className={`${styles.listItem} ${selectedIndex === index ? styles.listItemSelected : ''}`}
                onClick={() => setSelectedIndex(index)}
              >
                {path}
              </button>
            </li>
          ))}
        </ul>
        <div className={styles.buttonRow}>
          <button
            type="button"
            className={`${styles.actionBtn} ${selectedIndex !== null ? styles.actionBtnOpenReady : ''}`}
            disabled={selectedIndex === null}
            onClick={handleOpenRecent}
          >
            Open
          </button>
          <button type="button" className={styles.actionBtn} onClick={handlePickFolder}>
            Open...
          </button>
          <button type="button" className={styles.actionBtn}>
            Create
          </button>
        </div>
        <div className={styles.ideLinkRow}>
          <Link to="/ide" className={styles.ideLink}>
            Open IDE (mock)
          </Link>
        </div>
      </div>
    </div>
  );
}
