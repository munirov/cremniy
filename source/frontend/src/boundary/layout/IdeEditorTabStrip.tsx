import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type DragEvent,
  type KeyboardEvent,
  type WheelEvent,
} from 'react';

import { fileNameFromPath } from '@domain/workspace/paths';

import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

import styles from './IdeEditorTabStrip.module.css';

const DRAG_MIME = 'application/x-cremniy-tab';

export function IdeEditorTabStrip() {
  const {
    openFilePaths,
    activeFilePath,
    dirtyFilePaths,
    pinnedFilePaths,
    togglePinFilePath,
    reorderOpenFiles,
    activateOpenFile,
    closeOpenFile,
    closeOtherOpenFiles,
    closeAllOpenFiles,
  } = useIdeSession();
  const dirtyFiles = new Set(dirtyFilePaths);
  const navigatedFromKeyboardRef = useRef(false);
  const tabSelectRefs = useRef(new Map<string, HTMLButtonElement>());
  const [ctxMenu, setCtxMenu] = useState<{ x: number; y: number; path: string } | null>(null);

  // Pinned tabs render first (Qt parity: pinned-zone protection on drag too).
  const sortedPaths = useMemo(() => {
    const pinned: string[] = [];
    const rest: string[] = [];
    for (const p of openFilePaths) {
      if (pinnedFilePaths.has(p)) pinned.push(p);
      else rest.push(p);
    }
    return [...pinned, ...rest];
  }, [openFilePaths, pinnedFilePaths]);

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
      if (sortedPaths.length === 0) {
        return;
      }
      const idx = Math.max(0, Math.min(sortedPaths.length - 1, nextIdx));
      navigatedFromKeyboardRef.current = true;
      activateOpenFile(sortedPaths[idx]!);
    },
    [activateOpenFile, sortedPaths],
  );

  const onTabStripKeyDown = useCallback(
    (ev: KeyboardEvent<HTMLDivElement>) => {
      if (sortedPaths.length === 0) {
        return;
      }
      let idx =
        activeFilePath != null && activeFilePath !== '' ? sortedPaths.indexOf(activeFilePath) : -1;
      let handled = false;
      let nextIdx = idx;

      if (ev.key === 'ArrowRight') {
        handled = true;
        nextIdx = idx >= 0 ? (idx + 1) % sortedPaths.length : 0;
      } else if (ev.key === 'ArrowLeft') {
        handled = true;
        nextIdx =
          idx >= 0
            ? (idx - 1 + sortedPaths.length) % sortedPaths.length
            : sortedPaths.length - 1;
      } else if (ev.key === 'Home') {
        handled = true;
        nextIdx = 0;
      } else if (ev.key === 'End') {
        handled = true;
        nextIdx = sortedPaths.length - 1;
      }

      if (handled) {
        ev.preventDefault();
        activateByIndexAndMarkKeyboard(nextIdx);
      }
    },
    [activateByIndexAndMarkKeyboard, activeFilePath, sortedPaths],
  );

  const onTabStripWheel = useCallback(
    (ev: WheelEvent<HTMLDivElement>) => {
      if (!ev.altKey || sortedPaths.length < 2) {
        return;
      }
      const delta = ev.deltaY !== 0 ? ev.deltaY : ev.deltaX;
      if (delta === 0) {
        return;
      }
      const idx =
        activeFilePath != null && activeFilePath !== '' ? sortedPaths.indexOf(activeFilePath) : -1;
      const base = idx >= 0 ? idx : 0;
      const step = delta < 0 ? 1 : -1;
      const nextIdx = (base + step + sortedPaths.length) % sortedPaths.length;
      activateByIndexAndMarkKeyboard(nextIdx);
    },
    [activateByIndexAndMarkKeyboard, activeFilePath, sortedPaths],
  );

  // Drag-reorder. Pinned-zone protection: a pinned tab can't be dropped into
  // the unpinned zone and vice versa — drop is silently ignored.
  const onDragStart = useCallback((ev: DragEvent<HTMLDivElement>, path: string) => {
    ev.dataTransfer.setData(DRAG_MIME, path);
    ev.dataTransfer.effectAllowed = 'move';
  }, []);

  const onDragOver = useCallback((ev: DragEvent<HTMLDivElement>) => {
    if (ev.dataTransfer.types.includes(DRAG_MIME)) {
      ev.preventDefault();
      ev.dataTransfer.dropEffect = 'move';
    }
  }, []);

  const onDrop = useCallback(
    (ev: DragEvent<HTMLDivElement>, targetPath: string) => {
      const source = ev.dataTransfer.getData(DRAG_MIME);
      if (source === '' || source === targetPath) return;
      const sourcePinned = pinnedFilePaths.has(source);
      const targetPinned = pinnedFilePaths.has(targetPath);
      if (sourcePinned !== targetPinned) return; // cross-zone drop ignored.
      ev.preventDefault();
      const fromIdx = openFilePaths.indexOf(source);
      const toIdx = openFilePaths.indexOf(targetPath);
      reorderOpenFiles(fromIdx, toIdx);
    },
    [openFilePaths, pinnedFilePaths, reorderOpenFiles],
  );

  if (sortedPaths.length === 0) {
    return (
      <div aria-label="Open document tabs" className={styles.strip} role="toolbar">
        <span className={styles.emptyHint}>No open files</span>
      </div>
    );
  }

  return (
    <>
      <div
        aria-label="Open document tabs"
        className={styles.strip}
        role="tablist"
        onKeyDown={onTabStripKeyDown}
        onWheel={onTabStripWheel}
      >
        {sortedPaths.map((path) => {
          const label = fileNameFromPath(path) || path;
          const isActive = activeFilePath === path;
          const isDirty = dirtyFiles.has(path);
          const isPinned = pinnedFilePaths.has(path);
          return (
            <div
              key={path}
              className={`${styles.tab} ${isActive ? styles.tabActive : ''}`}
              data-open-file-path={path}
              role="presentation"
              draggable
              onDragStart={(e) => onDragStart(e, path)}
              onDragOver={onDragOver}
              onDrop={(e) => onDrop(e, path)}
              onContextMenu={(e) => {
                e.preventDefault();
                setCtxMenu({ x: e.clientX, y: e.clientY, path });
              }}
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
                <span className={styles.tabLabel}>
                  {isPinned ? '📌 ' : ''}
                  {isDirty ? `${label} *` : label}
                </span>
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

      {ctxMenu != null ? (
        <ul
          role="menu"
          style={{
            position: 'fixed',
            top: ctxMenu.y,
            left: ctxMenu.x,
            margin: 0,
            padding: '0.25rem 0',
            listStyle: 'none',
            background: 'var(--color-bg-panel)',
            border: '1px solid var(--color-border-pane)',
            borderRadius: 4,
            boxShadow: '0 0.5rem 1rem rgba(0,0,0,0.45)',
            zIndex: 100,
            minWidth: '12rem',
            fontSize: 13,
          }}
          onMouseLeave={() => setCtxMenu(null)}
        >
          {[
            {
              label: pinnedFilePaths.has(ctxMenu.path) ? 'Unpin tab' : 'Pin tab',
              onClick: () => {
                togglePinFilePath(ctxMenu.path);
                setCtxMenu(null);
              },
            },
            { label: 'Close', onClick: () => { closeOpenFile(ctxMenu.path); setCtxMenu(null); } },
            {
              label: 'Close others',
              onClick: () => {
                closeOtherOpenFiles(ctxMenu.path);
                setCtxMenu(null);
              },
            },
            { label: 'Close all', onClick: () => { closeAllOpenFiles(); setCtxMenu(null); } },
            {
              label: 'Copy path',
              onClick: () => {
                void navigator.clipboard.writeText(ctxMenu.path);
                setCtxMenu(null);
              },
            },
          ].map((it) => (
            <li role="none" key={it.label}>
              <button
                type="button"
                role="menuitem"
                style={{
                  display: 'block',
                  width: '100%',
                  padding: '0.3rem 0.65rem',
                  border: 'none',
                  background: 'transparent',
                  color: 'inherit',
                  textAlign: 'left',
                  font: 'inherit',
                  cursor: 'pointer',
                }}
                onClick={it.onClick}
              >
                {it.label}
              </button>
            </li>
          ))}
        </ul>
      ) : null}
    </>
  );
}
