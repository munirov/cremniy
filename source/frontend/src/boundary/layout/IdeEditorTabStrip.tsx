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
import { useRegistryVersion } from '@shared/plugins/useRegistry';

import { resolveCenterPanel } from './centerPanels';
import { useTabDrag } from './TabDragContext';
import styles from './IdeEditorTabStrip.module.css';

const DRAG_MIME = 'application/x-cremniy-tab';

/** The drag payload a tab carries on {@link DRAG_MIME}: which file, from where. */
type TabDragPayload = { path: string; sourceGroupId: string };

/** Parse a {@link DRAG_MIME} payload, tolerating the bare-path legacy form. */
function parseTabDrag(raw: string): TabDragPayload | null {
  if (raw === '') {
    return null;
  }
  try {
    const o = JSON.parse(raw) as Partial<TabDragPayload>;
    if (typeof o?.path === 'string' && typeof o?.sourceGroupId === 'string' && o.path !== '') {
      return { path: o.path, sourceGroupId: o.sourceGroupId };
    }
  } catch {
    // Not JSON — fall through (older drag that carried just the path).
  }
  return null;
}

export type IdeEditorTabStripProps = {
  /**
   * Which editor group this strip renders. Defaults to the active group, in
   * which case the strip behaves exactly like the pre-groups single strip. A
   * tab is shown active only when it's this group's active tab AND this group
   * is the focused one — so the inactive group's strip never shows a selection.
   */
  groupId?: string;
};

export function IdeEditorTabStrip({ groupId }: IdeEditorTabStripProps = {}) {
  const session = useIdeSession();
  const {
    dirtyFilePaths,
    pinnedFilePaths,
    togglePinFilePath,
    closeOtherOpenFiles,
    closeAllOpenFiles,
  } = session;
  const { beginTabDrag, endTabDrag } = useTabDrag();

  // Resolve the group this strip renders. When `groupId` is given, scope to that
  // group's view + per-group mutators; otherwise fall back to the active-group
  // projection (identical to the legacy single-group strip).
  const activeGroupId = session.activeGroupId;
  const targetGroupId = groupId ?? activeGroupId;
  const group = session.editorGroups?.find((g) => g.id === targetGroupId);
  const isActiveGroup = targetGroupId === activeGroupId;

  // View state for this group (group projection when scoped, else the top-level
  // active-group fields the context still exposes for backward compatibility).
  const openFilePaths = group?.openTabs ?? session.openFilePaths;
  const groupActiveFilePath = group?.activeFilePath ?? session.activeFilePath;
  const openPanels = group?.openPanels ?? session.openPanels;
  const groupActivePanel = group?.activePanel ?? session.activePanel;
  const previewFilePath = group?.previewFilePath ?? session.previewFilePath;
  // A tab/panel only reads as "selected" when it's active in its group AND that
  // group has focus — the unfocused group shows no active tab.
  const activeFilePath = isActiveGroup ? groupActiveFilePath : null;
  const activePanel = isActiveGroup ? groupActivePanel : null;

  // Per-group mutators, scoped to this strip's group.
  const activateOpenFile = useCallback(
    (path: string) => session.activateFileInGroup(targetGroupId, path),
    [session, targetGroupId],
  );
  const closeOpenFile = useCallback(
    (path: string) => session.closeFileInGroup(targetGroupId, path),
    [session, targetGroupId],
  );
  const reorderOpenFiles = useCallback(
    (from: number, to: number) => session.reorderFilesInGroup(targetGroupId, from, to),
    [session, targetGroupId],
  );
  const activatePanel = useCallback(
    (id: string) => session.activatePanelInGroup(targetGroupId, id),
    [session, targetGroupId],
  );
  const closePanel = useCallback(
    (id: string) => session.closePanelInGroup(targetGroupId, id),
    [session, targetGroupId],
  );
  useRegistryVersion(); // re-render so a disabled plugin's center-panel tab vanishes
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

  // Drag a tab. The payload carries the file path AND its source group so a drop
  // on another group's strip can MOVE it (cross-group), while a drop on the same
  // strip stays a reorder. A module-wide "dragging" flag (context) is raised so
  // the per-group edge drop-zones mount only during a drag.
  const onDragStart = useCallback(
    (ev: DragEvent<HTMLDivElement>, path: string) => {
      const payload: TabDragPayload = { path, sourceGroupId: targetGroupId };
      ev.dataTransfer.setData(DRAG_MIME, JSON.stringify(payload));
      ev.dataTransfer.effectAllowed = 'move';
      beginTabDrag();
    },
    [beginTabDrag, targetGroupId],
  );

  const onDragEnd = useCallback(() => {
    endTabDrag();
  }, [endTabDrag]);

  const onDragOver = useCallback((ev: DragEvent<HTMLDivElement>) => {
    if (ev.dataTransfer.types.includes(DRAG_MIME)) {
      ev.preventDefault();
      ev.dataTransfer.dropEffect = 'move';
    }
  }, []);

  const onDrop = useCallback(
    (ev: DragEvent<HTMLDivElement>, targetPath: string) => {
      const payload = parseTabDrag(ev.dataTransfer.getData(DRAG_MIME));
      if (payload == null) return;
      ev.preventDefault();
      ev.stopPropagation(); // handled here — don't also fire the strip-level drop.
      endTabDrag();
      const { path: source, sourceGroupId } = payload;

      // Cross-group: MOVE the file into this strip at the dropped tab's slot.
      // (The pure moveTabBetweenGroups de-dupes if it's already here, and
      // collapses the source group if this was its last tab.)
      if (sourceGroupId !== targetGroupId) {
        const at = openFilePaths.indexOf(targetPath);
        session.moveFileToGroup(sourceGroupId, targetGroupId, source, at < 0 ? undefined : at);
        return;
      }

      // Same group → reorder. Pinned-zone protection: a pinned tab can't move
      // into the unpinned zone and vice versa — that drop is silently ignored.
      if (source === targetPath) return;
      const sourcePinned = pinnedFilePaths.has(source);
      const targetPinned = pinnedFilePaths.has(targetPath);
      if (sourcePinned !== targetPinned) return;
      const fromIdx = openFilePaths.indexOf(source);
      const toIdx = openFilePaths.indexOf(targetPath);
      reorderOpenFiles(fromIdx, toIdx);
    },
    [endTabDrag, openFilePaths, pinnedFilePaths, reorderOpenFiles, session, targetGroupId],
  );

  // Drop in the strip's empty gutter (past the last tab). A cross-group drop
  // here MOVES the file into this group at the end; a same-group drop is a
  // no-op (it's already here). Per-tab drops stopPropagation, so this only runs
  // for the gutter.
  const onStripDrop = useCallback(
    (ev: DragEvent<HTMLDivElement>) => {
      const payload = parseTabDrag(ev.dataTransfer.getData(DRAG_MIME));
      if (payload == null) return;
      ev.preventDefault();
      endTabDrag();
      if (payload.sourceGroupId !== targetGroupId) {
        session.moveFileToGroup(payload.sourceGroupId, targetGroupId, payload.path);
      }
    },
    [endTabDrag, session, targetGroupId],
  );

  if (sortedPaths.length === 0 && openPanels.length === 0) {
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
        onDragOver={onDragOver}
        onDrop={onStripDrop}
      >
        {sortedPaths.map((path) => {
          const label = fileNameFromPath(path) || path;
          const isActive = activeFilePath === path && activePanel == null;
          const isDirty = dirtyFiles.has(path);
          const isPinned = pinnedFilePaths.has(path);
          const isPreview = previewFilePath === path;
          return (
            <div
              key={path}
              className={`${styles.tab} ${isActive ? styles.tabActive : ''}`}
              data-open-file-path={path}
              role="presentation"
              draggable
              onDragStart={(e) => onDragStart(e, path)}
              onDragEnd={onDragEnd}
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
                <span
                  className={`${styles.tabLabel}${isPreview ? ` ${styles.tabLabelPreview}` : ''}`}
                >
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
        {openPanels.map((id) => {
          const def = resolveCenterPanel(id);
          if (def == null) return null;
          const panelActive = activePanel === id;
          return (
            <div
              key={`panel:${id}`}
              className={`${styles.tab} ${panelActive ? styles.tabActive : ''}`}
              role="presentation"
            >
              <button
                type="button"
                role="tab"
                aria-selected={panelActive}
                tabIndex={panelActive ? 0 : -1}
                className={styles.tabSelectBtn}
                title={def.label}
                onClick={() => activatePanel(id)}
              >
                <span className={styles.tabLabel}>{def.label}</span>
              </button>
              <button
                type="button"
                aria-label={`Close ${def.label}`}
                className={styles.tabCloseBtn}
                tabIndex={-1}
                onClick={(e) => {
                  e.stopPropagation();
                  closePanel(id);
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
