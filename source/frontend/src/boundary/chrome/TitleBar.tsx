import { useEffect, useState, type ReactNode } from 'react';

import styles from './TitleBar.module.css';

/**
 * Cursor / VS Code style window chrome: a 36px strip pinned to the top of
 * the window that holds the product logo, the menu bar, the drag region for
 * moving the window, and three native-feeling window controls (minimize,
 * maximize / restore, close).
 *
 * Native OS decorations must be disabled in `tauri.conf.json`
 * (`decorations: false`) for this bar to be the only chrome. The window
 * controls call the Tauri webview API; the bar still renders fine in plain
 * browser previews — the buttons become no-ops in that case.
 */
export type TitleBarProps = {
  /** Slot for the menu bar (File / Edit / View / ...). */
  menu?: ReactNode;
  /** Optional centered area (e.g. quick search or breadcrumb). */
  center?: ReactNode;
  /** When set, a settings gear appears just left of the window controls. */
  onOpenSettings?: () => void;
};

export function TitleBar({ menu, center, onOpenSettings }: TitleBarProps) {
  const [isMaximized, setMaximized] = useState(false);
  const [tauriWindow, setTauriWindow] = useState<TauriWindowApi | null>(null);

  // Bind to the Tauri window asynchronously — in a browser preview the import
  // will throw on `getCurrentWindow()`, we just skip the controls then.
  useEffect(() => {
    let cancelled = false;
    void (async () => {
      try {
        const mod = await import('@tauri-apps/api/window');
        if (cancelled) return;
        const win = mod.getCurrentWindow();
        const api: TauriWindowApi = {
          minimize: () => win.minimize(),
          toggleMaximize: () => win.toggleMaximize(),
          close: () => win.close(),
          isMaximized: () => win.isMaximized(),
        };
        setTauriWindow(api);
        const max = await win.isMaximized();
        if (!cancelled) setMaximized(max);
        // Listen for resize → re-check maximized state.
        const unlisten = await win.onResized(async () => {
          if (cancelled) return;
          setMaximized(await win.isMaximized());
        });
        if (cancelled) unlisten();
      } catch {
        // Browser preview (no Tauri context) — leave controls as no-ops.
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  const handleMinimize = () => void tauriWindow?.minimize();
  const handleToggleMaximize = () => void tauriWindow?.toggleMaximize();
  const handleClose = () => void tauriWindow?.close();

  return (
    <div className={styles.bar} data-tauri-drag-region>
      <div className={styles.brand} data-tauri-drag-region>
        <img
          className={styles.logo}
          src="/cremniy-logo.svg"
          alt=""
          aria-hidden
          draggable={false}
        />
        <span className={styles.brandName}>Cremniy</span>
      </div>
      <div className={styles.menuSlot}>{menu}</div>
      <div className={styles.dragArea} data-tauri-drag-region />
      {center != null ? <div className={styles.centerSlot}>{center}</div> : null}
      <div className={styles.controls} aria-label="Window controls">
        {onOpenSettings != null ? (
          <button
            type="button"
            className={styles.ctrlBtn}
            onClick={onOpenSettings}
            title="Settings"
            aria-label="Settings"
          >
            <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" aria-hidden>
              <path d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z" />
              <circle cx="12" cy="12" r="3" />
            </svg>
          </button>
        ) : null}
        <button
          type="button"
          className={styles.ctrlBtn}
          onClick={handleMinimize}
          title="Minimize"
          aria-label="Minimize"
        >
          <svg width="12" height="12" viewBox="0 0 12 12" aria-hidden>
            <rect x="2" y="6" width="8" height="1" fill="currentColor" />
          </svg>
        </button>
        <button
          type="button"
          className={styles.ctrlBtn}
          onClick={handleToggleMaximize}
          title={isMaximized ? 'Restore' : 'Maximize'}
          aria-label={isMaximized ? 'Restore' : 'Maximize'}
        >
          {isMaximized ? (
            // Restore — two layered squares (the reference shot: a small
            // square with a corner peeking out from behind it).
            <svg
              width="12"
              height="12"
              viewBox="0 0 12 12"
              aria-hidden
              fill="none"
              stroke="currentColor"
              strokeWidth="1"
              strokeLinecap="square"
              strokeLinejoin="miter"
            >
              {/* Back square — only the right + top edges show. */}
              <path d="M4 3 L9 3 L9 8" />
              {/* Front square — full rectangle, slightly down-left. */}
              <rect x="2" y="4" width="6" height="6" />
            </svg>
          ) : (
            // Maximize — single empty square taking most of the icon area.
            <svg
              width="13"
              height="13"
              viewBox="0 0 13 13"
              aria-hidden
              fill="none"
              stroke="currentColor"
              strokeWidth="1"
              strokeLinecap="square"
              strokeLinejoin="miter"
            >
              <rect x="2" y="2" width="9" height="9" />
            </svg>
          )}
        </button>
        <button
          type="button"
          className={`${styles.ctrlBtn} ${styles.ctrlBtnClose}`}
          onClick={handleClose}
          title="Close"
          aria-label="Close"
        >
          <svg width="14" height="14" viewBox="0 0 14 14" aria-hidden fill="none" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round">
            <path d="M3 3l8 8M11 3l-8 8" />
          </svg>
        </button>
      </div>
    </div>
  );
}

type TauriWindowApi = {
  minimize: () => Promise<void>;
  toggleMaximize: () => Promise<void>;
  close: () => Promise<void>;
  isMaximized: () => Promise<boolean>;
};
