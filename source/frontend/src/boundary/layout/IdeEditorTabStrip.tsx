import { useCallback, useEffect, useRef, type KeyboardEvent, type WheelEvent } from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';

import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

import styles from './IdeEditorTabStrip.module.css';

export function IdeEditorTabStrip() {
  const { openFilePaths, activeFilePath, dirtyFilePaths, activateOpenFile, closeOpenFile } = useIdeSession();
  const dirtyFiles = new Set(dirtyFilePaths);
  const navigatedFromKeyboardRef = useRef(false);
  const tabSelectRefs = useRef(new Map<string, HTMLButtonElement>());

  useEffect(() => {
    if (!navigatedFromKeyboardRef.current || activeFilePath == null || activeFilePath === '') {
      return;
    }
    navigatedFromKeyboardRef.current = false;
    queueMicrotask(() => {
      tabSelectRefs.current.get(activeFilePath)?.focus();
    });
  }, [activeFilePath]);

  const activateByIndexAndMarkKeyboard = useCallback(
    (nextIdx: number) => {
      if (openFilePaths.length === 0) {
        return;
      }
      const idx = Math.max(0, Math.min(openFilePaths.length - 1, nextIdx));
      navigatedFromKeyboardRef.current = true;
      activateOpenFile(openFilePaths[idx]!);
    },
    [activateOpenFile, openFilePaths],
  );

  const onTabStripKeyDown = useCallback(
    (ev: KeyboardEvent<HTMLDivElement>) => {
      if (openFilePaths.length === 0) {
        return;
      }
      let idx =
        activeFilePath != null && activeFilePath !== '' ? openFilePaths.indexOf(activeFilePath) : -1;
      let handled = false;
      let nextIdx = idx;

      if (ev.key === 'ArrowRight') {
        handled = true;
        if (idx >= 0) {
          nextIdx = (idx + 1) % openFilePaths.length;
        } else {
          nextIdx = 0;
        }
      } else if (ev.key === 'ArrowLeft') {
        handled = true;
        if (idx >= 0) {
          nextIdx = (idx - 1 + openFilePaths.length) % openFilePaths.length;
        } else {
          nextIdx = openFilePaths.length - 1;
        }
      } else if (ev.key === 'Home') {
        handled = true;
        nextIdx = 0;
      } else if (ev.key === 'End') {
        handled = true;
        nextIdx = openFilePaths.length - 1;
      }

      if (handled) {
        ev.preventDefault();
        activateByIndexAndMarkKeyboard(nextIdx);
      }
    },
    [activateByIndexAndMarkKeyboard, activeFilePath, openFilePaths],
  );

  // Qt parity: FilesTabWidget event filter switched tabs on Alt+Wheel.
  const onTabStripWheel = useCallback(
    (ev: WheelEvent<HTMLDivElement>) => {
      if (!ev.altKey || openFilePaths.length < 2) {
        return;
      }
      const delta = ev.deltaY !== 0 ? ev.deltaY : ev.deltaX;
      if (delta === 0) {
        return;
      }
      const idx =
        activeFilePath != null && activeFilePath !== '' ? openFilePaths.indexOf(activeFilePath) : -1;
      const base = idx >= 0 ? idx : 0;
      const step = delta < 0 ? 1 : -1;
      const nextIdx = (base + step + openFilePaths.length) % openFilePaths.length;
      activateByIndexAndMarkKeyboard(nextIdx);
    },
    [activateByIndexAndMarkKeyboard, activeFilePath, openFilePaths],
  );

  if (openFilePaths.length === 0) {
    return (
      <div aria-label="Open document tabs" className={styles.strip} role="toolbar">
        <span className={styles.emptyHint}>No open files</span>
      </div>
    );
  }

  return (
    <div
      aria-label="Open document tabs"
      className={styles.strip}
      role="tablist"
      onKeyDown={onTabStripKeyDown}
      onWheel={onTabStripWheel}
    >
      {openFilePaths.map((path) => {
        const label = fileNameFromPath(path) || path;
        const isActive = activeFilePath === path;
        const isDirty = dirtyFiles.has(path);
        return (
          <div
            key={path}
            className={`${styles.tab} ${isActive ? styles.tabActive : ''}`}
            data-open-file-path={path}
            role="presentation"
            onAuxClick={(e) => {
              if (e.button === 1) {
                e.preventDefault();
                closeOpenFile(path);
              }
            }}
          >
            <button
              type="button"
              ref={(el) => {
                if (el == null) {
                  tabSelectRefs.current.delete(path);
                } else {
                  tabSelectRefs.current.set(path, el);
                }
              }}
              role="tab"
              aria-selected={isActive}
              tabIndex={isActive ? 0 : -1}
              className={styles.tabSelectBtn}
              title={path}
              onClick={() => activateOpenFile(path)}
            >
              <span className={styles.tabLabel}>{isDirty ? `${label} *` : label}</span>
            </button>
            <button
              type="button"
              aria-label={`Close ${label}`}
              className={styles.tabCloseBtn}
              tabIndex={-1}
              onClick={(e) => {
                e.stopPropagation();
                closeOpenFile(path);
              }}
            >
              ×
            </button>
          </div>
        );
      })}
    </div>
  );
}
