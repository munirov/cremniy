import { useCallback, useEffect, useRef, useState } from 'react';
import type { LegacyRef, MouseEvent } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import type { WorkspaceDirectoryEntry } from '@domain/workspace/directoryEntry';
import { fileNameFromPath, joinFilePath, parentDirectoryPath } from '@domain/workspace/paths';
import { validateWorkspaceEntryName } from '@domain/workspace/workspaceEntryValidation';
import {
  createDirectoryUnderWorkspace,
  createEmptyFileUnderWorkspace,
  deleteUnderWorkspace,
  listDirectoryEntries,
  renameUnderWorkspace,
} from '@infrastructure/tauri/bridge';

import { useIdeSession } from './IdeSessionContext';

import styles from './WorkspaceFileTree.module.css';

type WorkspaceFileTreeProps = {
  workspaceRoot: WorkspaceRoot | null;
};

type CtxState = {
  clientX: number;
  clientY: number;
  path: string;
  isDirectory: boolean;
};

function formatErr(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

export function WorkspaceFileTree({ workspaceRoot }: WorkspaceFileTreeProps) {
  const { activeFilePath, openFileFromWorkspace, fileTreeRevision, bumpFileTreeRevision } = useIdeSession();
  const [entries, setEntries] = useState<WorkspaceDirectoryEntry[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [ctx, setCtx] = useState<CtxState | null>(null);
  const ctxMenuRef = useRef<HTMLUListElement | null>(null);

  useEffect(() => {
    if (workspaceRoot == null || workspaceRoot.path === '') {
      setEntries(null);
      setError(null);
      setLoading(false);
      return;
    }

    let cancelled = false;
    setLoading(true);
    setError(null);
    setEntries(null);

    void listDirectoryEntries(workspaceRoot.path, workspaceRoot.path)
      .then((list) => {
        if (!cancelled) {
          setEntries(list);
        }
      })
      .catch((e: unknown) => {
        if (!cancelled) {
          setError(e instanceof Error ? e.message : String(e));
        }
      })
      .finally(() => {
        if (!cancelled) {
          setLoading(false);
        }
      });

    return () => {
      cancelled = true;
    };
  }, [workspaceRoot, fileTreeRevision]);

  useEffect(() => {
    if (ctx == null) {
      return;
    }
    const onKey = (ev: KeyboardEvent) => {
      if (ev.key === 'Escape') {
        setCtx(null);
      }
    };
    const onDown = (ev: PointerEvent) => {
      const el = ctxMenuRef.current;
      if (el != null && !el.contains(ev.target as Node)) {
        setCtx(null);
      }
    };
    window.addEventListener('keydown', onKey);
    window.addEventListener('pointerdown', onDown);
    return () => {
      window.removeEventListener('keydown', onKey);
      window.removeEventListener('pointerdown', onDown);
    };
  }, [ctx]);

  const openCtx = useCallback((ev: MouseEvent, path: string, isDirectory: boolean) => {
    ev.preventDefault();
    ev.stopPropagation();
    setCtx({ clientX: ev.clientX, clientY: ev.clientY, path, isDirectory });
  }, []);

  const runNewFile = useCallback(async () => {
    if (ctx == null || workspaceRoot == null) {
      return;
    }
    const dirPath = ctx.isDirectory ? ctx.path : parentDirectoryPath(ctx.path);
    if (dirPath === '') {
      window.alert('Could not resolve parent directory.');
      return;
    }
    const name = window.prompt('New file name');
    if (name == null) {
      return;
    }
    const err = validateWorkspaceEntryName(name);
    if (err != null) {
      window.alert(err);
      return;
    }
    const full = joinFilePath(dirPath, name.trim());
    try {
      await createEmptyFileUnderWorkspace(workspaceRoot.path, full);
      bumpFileTreeRevision();
      setCtx(null);
    } catch (e) {
      window.alert(formatErr(e));
    }
  }, [bumpFileTreeRevision, ctx, workspaceRoot]);

  const runNewFolder = useCallback(async () => {
    if (ctx == null || workspaceRoot == null) {
      return;
    }
    const dirPath = ctx.isDirectory ? ctx.path : parentDirectoryPath(ctx.path);
    if (dirPath === '') {
      window.alert('Could not resolve parent directory.');
      return;
    }
    const name = window.prompt('New folder name');
    if (name == null) {
      return;
    }
    const err = validateWorkspaceEntryName(name);
    if (err != null) {
      window.alert(err);
      return;
    }
    const full = joinFilePath(dirPath, name.trim());
    try {
      await createDirectoryUnderWorkspace(workspaceRoot.path, full);
      bumpFileTreeRevision();
      setCtx(null);
    } catch (e) {
      window.alert(formatErr(e));
    }
  }, [bumpFileTreeRevision, ctx, workspaceRoot]);

  const runRename = useCallback(async () => {
    if (ctx == null || workspaceRoot == null) {
      return;
    }
    const oldPath = ctx.path;
    const parent = parentDirectoryPath(oldPath);
    const last = fileNameFromPath(oldPath);
    if (parent === '' || last === '') {
      window.alert('Could not rename this path.');
      return;
    }
    const name = window.prompt('New name', last);
    if (name == null) {
      return;
    }
    const err = validateWorkspaceEntryName(name);
    if (err != null) {
      window.alert(err);
      return;
    }
    const newPath = joinFilePath(parent, name.trim());
    if (newPath === oldPath) {
      setCtx(null);
      return;
    }
    try {
      await renameUnderWorkspace(workspaceRoot.path, oldPath, newPath);
      bumpFileTreeRevision();
      setCtx(null);
    } catch (e) {
      window.alert(formatErr(e));
    }
  }, [bumpFileTreeRevision, ctx, workspaceRoot]);

  const runDelete = useCallback(async () => {
    if (ctx == null || workspaceRoot == null) {
      return;
    }
    const target = ctx.path;
    if (!window.confirm(`Delete ${ctx.isDirectory ? 'folder' : 'file'}?\n${target}`)) {
      return;
    }
    try {
      await deleteUnderWorkspace(workspaceRoot.path, target);
      bumpFileTreeRevision();
      setCtx(null);
    } catch (e) {
      window.alert(formatErr(e));
    }
  }, [bumpFileTreeRevision, ctx, workspaceRoot]);

  if (workspaceRoot == null || workspaceRoot.path === '') {
    return (
      <div className={styles.stateBox} role="status">
        <p className={styles.stateLine}>No workspace folder open.</p>
        <p className={styles.stateHint}>Use File → Open folder.</p>
      </div>
    );
  }

  if (loading && entries === null && error === null) {
    return (
      <div className={styles.stateBox} role="status" aria-busy="true">
        <p className={styles.stateLine}>Loading files…</p>
      </div>
    );
  }

  if (error != null) {
    return (
      <div className={styles.stateBox} role="alert">
        <p className={styles.stateLine}>Could not list workspace.</p>
        <p className={styles.stateDetail}>{error}</p>
      </div>
    );
  }

  if (entries != null && entries.length === 0) {
    return (
      <>
        <div
          className={styles.treeRoot}
          role="tree"
          aria-label="Workspace files"
          onContextMenu={(e) => openCtx(e, workspaceRoot.path, true)}
        >
          <div role="status">
            <p className={styles.stateLine}>This folder is empty.</p>
            <p className={styles.stateHint}>Right-click for new file or folder.</p>
          </div>
        </div>
        {ctx != null ? (
          <CtxMenu
            menuRef={ctxMenuRef}
            x={ctx.clientX}
            y={ctx.clientY}
            onNewFile={() => void runNewFile()}
            onNewFolder={() => void runNewFolder()}
            onRename={() => void runRename()}
            onDelete={() => void runDelete()}
          />
        ) : null}
      </>
    );
  }

  return (
    <>
      <div className={styles.treeRoot} role="tree" aria-label="Workspace files">
        <ul
          className={styles.treeList}
          role="group"
          onContextMenu={(e) => {
            if (e.target === e.currentTarget) {
              e.preventDefault();
              openCtx(e, workspaceRoot.path, true);
            }
          }}
        >
          {entries?.map((entry) => (
            <li key={entry.path} className={styles.treeItem} role="none">
              <FileTreeNode
                workspaceRootPath={workspaceRoot.path}
                depth={0}
                entry={entry}
                activeFilePath={activeFilePath}
                onOpenFile={openFileFromWorkspace}
                onContextMenu={openCtx}
              />
            </li>
          ))}
        </ul>
      </div>
      {ctx != null ? (
        <CtxMenu
          menuRef={ctxMenuRef}
          x={ctx.clientX}
          y={ctx.clientY}
          onNewFile={() => void runNewFile()}
          onNewFolder={() => void runNewFolder()}
          onRename={() => void runRename()}
          onDelete={() => void runDelete()}
        />
      ) : null}
    </>
  );
}

type CtxMenuProps = {
  menuRef: LegacyRef<HTMLUListElement>;
  x: number;
  y: number;
  onNewFile: () => void;
  onNewFolder: () => void;
  onRename: () => void;
  onDelete: () => void;
};

function CtxMenu({ menuRef, x, y, onNewFile, onNewFolder, onRename, onDelete }: CtxMenuProps) {
  return (
    <ul ref={menuRef} className={styles.ctxMenu} style={{ left: x, top: y }} role="menu" data-testid="file-tree-ctx-menu">
      <li role="none">
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onNewFile}>
          New file…
        </button>
      </li>
      <li role="none">
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onNewFolder}>
          New folder…
        </button>
      </li>
      <li role="none">
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onRename}>
          Rename…
        </button>
      </li>
      <li role="none">
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onDelete}>
          Delete…
        </button>
      </li>
    </ul>
  );
}

type FileTreeNodeProps = {
  workspaceRootPath: string;
  depth: number;
  entry: WorkspaceDirectoryEntry;
  activeFilePath: string | null;
  onOpenFile: (path: string) => Promise<void>;
  onContextMenu: (ev: MouseEvent, path: string, isDirectory: boolean) => void;
};

function FileTreeNode({
  workspaceRootPath,
  depth,
  entry,
  activeFilePath,
  onOpenFile,
  onContextMenu,
}: FileTreeNodeProps) {
  const [expanded, setExpanded] = useState(false);
  const [children, setChildren] = useState<WorkspaceDirectoryEntry[] | null>(null);
  const [childError, setChildError] = useState<string | null>(null);
  const [loadingChildren, setLoadingChildren] = useState(false);

  const paddingRem = 0.35 + depth * 0.65;

  const loadChildren = useCallback(async () => {
    setLoadingChildren(true);
    setChildError(null);
    try {
      const list = await listDirectoryEntries(workspaceRootPath, entry.path);
      setChildren(list);
    } catch (e: unknown) {
      setChildError(e instanceof Error ? e.message : String(e));
    } finally {
      setLoadingChildren(false);
    }
  }, [workspaceRootPath, entry.path]);

  const toggleDir = useCallback(() => {
    if (!entry.isDirectory) {
      return;
    }
    if (!expanded) {
      void loadChildren();
    }
    setExpanded((v) => !v);
  }, [entry.isDirectory, expanded, loadChildren]);

  if (!entry.isDirectory) {
    const isActive = activeFilePath === entry.path;
    return (
      <button
        type="button"
        role="treeitem"
        aria-selected={isActive}
        className={`${styles.leafButton} ${isActive ? styles.leafButtonActive : ''}`}
        style={{ paddingLeft: `${paddingRem}rem` }}
        onClick={() => void onOpenFile(entry.path)}
        onContextMenu={(e) => onContextMenu(e, entry.path, false)}
      >
        <span className={styles.leafIcon} aria-hidden>
          {' '}
        </span>
        <span className={styles.leafName}>{entry.name}</span>
      </button>
    );
  }

  return (
    <div className={styles.dirBlock}>
      <button
        type="button"
        role="treeitem"
        aria-expanded={expanded}
        className={styles.dirRow}
        style={{ paddingLeft: `${paddingRem}rem` }}
        onClick={() => toggleDir()}
        onContextMenu={(e) => onContextMenu(e, entry.path, true)}
      >
        <span className={styles.chevron} aria-hidden>
          {expanded ? '▼' : '▶'}
        </span>
        <span className={styles.dirName}>{entry.name}</span>
        {loadingChildren ? <span className={styles.dirBusy}> …</span> : null}
      </button>
      {childError != null ? (
        <p className={styles.childError} role="alert" style={{ paddingLeft: `${paddingRem + 1}rem` }}>
          {childError}
        </p>
      ) : null}
      {expanded && children != null ? (
        <ul className={styles.nestedList} role="group">
          {children.map((child) => (
            <li key={child.path} className={styles.treeItem} role="none">
              <FileTreeNode
                workspaceRootPath={workspaceRootPath}
                depth={depth + 1}
                entry={child}
                activeFilePath={activeFilePath}
                onOpenFile={onOpenFile}
                onContextMenu={onContextMenu}
              />
            </li>
          ))}
        </ul>
      ) : null}
    </div>
  );
}
