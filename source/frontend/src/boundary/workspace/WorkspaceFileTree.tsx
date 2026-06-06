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
  revealInFileManager,
} from '@infrastructure/tauri/bridge';
import { loadPreferences } from '@infrastructure/preferences/preferencesBridge';

import { useNotify } from '@boundary/notifications/NotificationContext';

import { useIdeSession } from './IdeSessionContext';
import { ChevronIcon, FileIcon, FolderIcon } from './fileIcons';

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
  const notify = useNotify();
  const [entries, setEntries] = useState<WorkspaceDirectoryEntry[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [ctx, setCtx] = useState<CtxState | null>(null);
  const [excludedPatterns, setExcludedPatterns] = useState<readonly string[]>([]);
  // Inline-create row state. When non-null, the tree renders an input under
  // the named parent (or at the root) so the user can type the new name
  // without a native window.prompt dialog.
  const [pendingCreate, setPendingCreate] = useState<{
    kind: 'file' | 'folder';
    parentPath: string;
  } | null>(null);
  // MUST be declared above any early-return below — see the Rules of Hooks
  // bug we just fixed in FileTreeNode. This drives the root-zone drop-target.
  const [rootDragOver, setRootDragOver] = useState(false);
  // Bumped by the header's "Collapse folders" button; every FileTreeNode folds.
  const [collapseSignal, setCollapseSignal] = useState(0);
  const ctxMenuRef = useRef<HTMLUListElement | null>(null);
  // Filter UI was removed; keep the tree's filter plumbing inert.
  const filterLower = '';

  // Load excluded-patterns from Settings once per workspace switch. The
  // textarea is one-pattern-per-line; empty lines are dropped.
  useEffect(() => {
    let cancelled = false;
    void loadPreferences().then((p) => {
      if (cancelled) return;
      const patterns = p.excludedFilePatterns
        .split(/\r?\n/)
        .map((s) => s.trim())
        .filter((s) => s !== '');
      setExcludedPatterns(patterns);
    });
    return () => {
      cancelled = true;
    };
  }, [workspaceRoot?.path]);

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

  const runNewFile = useCallback(() => {
    if (ctx == null || workspaceRoot == null) return;
    const dirPath = ctx.isDirectory ? ctx.path : parentDirectoryPath(ctx.path);
    if (dirPath === '') {
      notify.error('Could not resolve parent directory.');
      return;
    }
    setPendingCreate({ kind: 'file', parentPath: dirPath });
    setCtx(null);
  }, [ctx, notify, workspaceRoot]);

  // Commit the inline-create input. The tree's input row calls this with the
  // typed name; we validate, hit the backend, and bump revision so the new
  // entry appears in the next list-directory cycle.
  const commitPendingCreate = useCallback(
    async (name: string) => {
      if (pendingCreate == null || workspaceRoot == null) return;
      const trimmed = name.trim();
      if (trimmed === '') {
        setPendingCreate(null);
        return;
      }
      const err = validateWorkspaceEntryName(trimmed);
      if (err != null) {
        notify.warn(err);
        return;
      }
      const full = joinFilePath(pendingCreate.parentPath, trimmed);
      try {
        if (pendingCreate.kind === 'file') {
          await createEmptyFileUnderWorkspace(workspaceRoot.path, full);
        } else {
          await createDirectoryUnderWorkspace(workspaceRoot.path, full);
        }
        bumpFileTreeRevision();
        setPendingCreate(null);
      } catch (e) {
        notify.error('Could not create', formatErr(e));
      }
    },
    [bumpFileTreeRevision, notify, pendingCreate, workspaceRoot],
  );

  const cancelPendingCreate = useCallback(() => setPendingCreate(null), []);

  // Header actions (create at the workspace root, refresh, collapse everything).
  const newFileAtRoot = useCallback(() => {
    if (workspaceRoot != null) setPendingCreate({ kind: 'file', parentPath: workspaceRoot.path });
  }, [workspaceRoot]);
  const newFolderAtRoot = useCallback(() => {
    if (workspaceRoot != null) setPendingCreate({ kind: 'folder', parentPath: workspaceRoot.path });
  }, [workspaceRoot]);
  const collapseAll = useCallback(() => setCollapseSignal((n) => n + 1), []);

  const runNewFolder = useCallback(() => {
    if (ctx == null || workspaceRoot == null) return;
    const dirPath = ctx.isDirectory ? ctx.path : parentDirectoryPath(ctx.path);
    if (dirPath === '') {
      notify.error('Could not resolve parent directory.');
      return;
    }
    setPendingCreate({ kind: 'folder', parentPath: dirPath });
    setCtx(null);
  }, [ctx, notify, workspaceRoot]);

  const runRename = useCallback(async () => {
    if (ctx == null || workspaceRoot == null) {
      return;
    }
    const oldPath = ctx.path;
    const parent = parentDirectoryPath(oldPath);
    const last = fileNameFromPath(oldPath);
    if (parent === '' || last === '') {
      notify.error('Could not rename this path.');
      return;
    }
    const name = window.prompt('New name', last);
    if (name == null) {
      return;
    }
    const err = validateWorkspaceEntryName(name);
    if (err != null) {
      notify.warn(err);
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
      notify.error('Operation failed', formatErr(e));
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
      notify.error('Operation failed', formatErr(e));
    }
  }, [bumpFileTreeRevision, ctx, workspaceRoot]);

  const runCopyPath = useCallback(async () => {
    if (ctx == null) {
      return;
    }
    try {
      await navigator.clipboard.writeText(ctx.path);
      setCtx(null);
    } catch (e) {
      notify.error('Operation failed', formatErr(e));
    }
  }, [ctx]);

  const runCopyRelativePath = useCallback(async () => {
    if (ctx == null || workspaceRoot == null) {
      return;
    }
    const root = workspaceRoot.path;
    const rel = ctx.path.startsWith(root)
      ? ctx.path.slice(root.length).replace(/^[\\/]+/, '')
      : ctx.path;
    try {
      await navigator.clipboard.writeText(rel);
      setCtx(null);
    } catch (e) {
      notify.error('Operation failed', formatErr(e));
    }
  }, [ctx, workspaceRoot]);

  const runRevealInExplorer = useCallback(async () => {
    if (ctx == null) return;
    try {
      await revealInFileManager(ctx.path);
      setCtx(null);
    } catch (e) {
      notify.error('Operation failed', formatErr(e));
    }
  }, [ctx]);

  const handleMoveInto = useCallback(
    async (sourcePath: string, targetDirectory: string) => {
      if (workspaceRoot == null) {
        return;
      }
      const name = fileNameFromPath(sourcePath);
      const sep = targetDirectory.includes('\\') ? '\\' : '/';
      const targetPath = `${targetDirectory}${sep}${name}`;
      if (targetPath === sourcePath) {
        return;
      }
      try {
        await renameUnderWorkspace(workspaceRoot.path, sourcePath, targetPath);
        bumpFileTreeRevision();
        notify.success(`Moved “${name}”`, targetDirectory);
      } catch (e) {
        notify.error('Move failed', formatErr(e));
      }
    },
    [bumpFileTreeRevision, notify, workspaceRoot],
  );

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
        <TreeHeader
          name={fileNameFromPath(workspaceRoot.path) || workspaceRoot.path}
          onNewFile={newFileAtRoot}
          onNewFolder={newFolderAtRoot}
          onRefresh={bumpFileTreeRevision}
          onCollapseAll={collapseAll}
        />
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
            onCopyPath={() => void runCopyPath()}
            onCopyRelativePath={() => void runCopyRelativePath()}
            onRevealInExplorer={() => void runRevealInExplorer()}
            onRename={() => void runRename()}
            onDelete={() => void runDelete()}
          />
        ) : null}
      </>
    );
  }

  const visibleEntries =
    entries == null
      ? []
      : entries.filter(
          (e) =>
            !excludedPatterns.some((p) => e.name.toLowerCase().includes(p.toLowerCase())),
        );

  // Root-level drop target — lets the user move a nested file BACK to the
  // workspace root by dragging it onto empty space at the top of the tree.
  // Plain HTML5 DnD; we only react if the payload carries our private MIME.
  const onRootDragOver = (e: React.DragEvent<HTMLElement>) => {
    if (e.dataTransfer.types.includes(DRAG_MIME)) {
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
      setRootDragOver(true);
    }
  };
  const onRootDragLeave = () => setRootDragOver(false);
  const onRootDrop = (e: React.DragEvent<HTMLElement>) => {
    setRootDragOver(false);
    const source = e.dataTransfer.getData(DRAG_MIME);
    if (source === '') return;
    // Already at root? Skip.
    if (parentDirectoryPath(source) === workspaceRoot.path) return;
    e.preventDefault();
    // stopPropagation: a drop on a nested folder bubbles up to here. Without
    // this guard the same file would be "renamed" twice — the second call
    // would fail with "from_path not found" because the first one already
    // moved it.
    e.stopPropagation();
    void handleMoveInto(source, workspaceRoot.path);
  };

  return (
    <>
      <div className={styles.section}>
        <TreeHeader
          name={fileNameFromPath(workspaceRoot.path) || workspaceRoot.path}
          onNewFile={newFileAtRoot}
          onNewFolder={newFolderAtRoot}
          onRefresh={bumpFileTreeRevision}
          onCollapseAll={collapseAll}
        />
        <div
          className={styles.treeRoot}
        role="tree"
        aria-label="Workspace files"
        onContextMenu={(e) => openCtx(e, workspaceRoot.path, true)}
        onDragOver={onRootDragOver}
        onDragLeave={onRootDragLeave}
        onDrop={onRootDrop}
        style={{
          outline: rootDragOver ? '1px dashed rgba(255,255,255,0.28)' : undefined,
          background: rootDragOver ? 'rgba(255,255,255,0.05)' : undefined,
        }}
      >
        <ul className={styles.treeList} role="group">
          {pendingCreate != null && pendingCreate.parentPath === workspaceRoot.path ? (
            <li className={styles.treeItem} role="none">
              <InlineCreateRow
                kind={pendingCreate.kind}
                depth={0}
                onSubmit={(name) => void commitPendingCreate(name)}
                onCancel={cancelPendingCreate}
              />
            </li>
          ) : null}
          {visibleEntries.map((entry) => (
            <li key={entry.path} className={styles.treeItem} role="none">
              <FileTreeNode
                workspaceRootPath={workspaceRoot.path}
                depth={0}
                entry={entry}
                activeFilePath={activeFilePath}
                filterLower={filterLower}
                excludedPatterns={excludedPatterns}
                pendingCreate={pendingCreate}
                collapseSignal={collapseSignal}
                onCommitCreate={commitPendingCreate}
                onCancelCreate={cancelPendingCreate}
                onOpenFile={openFileFromWorkspace}
                onContextMenu={openCtx}
                onMoveInto={handleMoveInto}
              />
            </li>
          ))}
        </ul>
        </div>
      </div>
      {ctx != null ? (
        <CtxMenu
          menuRef={ctxMenuRef}
          x={ctx.clientX}
          y={ctx.clientY}
          onNewFile={() => void runNewFile()}
          onNewFolder={() => void runNewFolder()}
          onCopyPath={() => void runCopyPath()}
          onCopyRelativePath={() => void runCopyRelativePath()}
          onRevealInExplorer={() => void runRevealInExplorer()}
          onRename={() => void runRename()}
          onDelete={() => void runDelete()}
        />
      ) : null}
    </>
  );
}

type TreeHeaderProps = {
  name: string;
  onNewFile: () => void;
  onNewFolder: () => void;
  onRefresh: () => void;
  onCollapseAll: () => void;
};

/**
 * Explorer section header: the project name, with new-file / new-folder /
 * refresh / collapse-all actions that fade in on hover (VS Code-style).
 */
function TreeHeader({ name, onNewFile, onNewFolder, onRefresh, onCollapseAll }: TreeHeaderProps) {
  return (
    <div className={styles.header}>
      <span className={styles.headerName} title={name}>
        {name}
      </span>
      <div className={styles.headerActions}>
        <button type="button" className={styles.headerBtn} title="New file" aria-label="New file" onClick={onNewFile}>
          <span className="codicon codicon-new-file" aria-hidden />
        </button>
        <button type="button" className={styles.headerBtn} title="New folder" aria-label="New folder" onClick={onNewFolder}>
          <span className="codicon codicon-new-folder" aria-hidden />
        </button>
        <button type="button" className={styles.headerBtn} title="Refresh explorer" aria-label="Refresh explorer" onClick={onRefresh}>
          <span className="codicon codicon-refresh" aria-hidden />
        </button>
        <button type="button" className={styles.headerBtn} title="Collapse folders" aria-label="Collapse folders" onClick={onCollapseAll}>
          <span className="codicon codicon-collapse-all" aria-hidden />
        </button>
      </div>
    </div>
  );
}

type CtxMenuProps = {
  menuRef: LegacyRef<HTMLUListElement>;
  x: number;
  y: number;
  onNewFile: () => void;
  onNewFolder: () => void;
  onCopyPath: () => void;
  onCopyRelativePath: () => void;
  onRevealInExplorer: () => void;
  onRename: () => void;
  onDelete: () => void;
};

function CtxMenu({
  menuRef,
  x,
  y,
  onNewFile,
  onNewFolder,
  onCopyPath,
  onCopyRelativePath,
  onRevealInExplorer,
  onRename,
  onDelete,
}: CtxMenuProps) {
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
      <li role="none" aria-hidden="true" className={styles.ctxMenuSeparator} />
      <li role="none">
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onCopyPath}>
          Copy Path
        </button>
      </li>
      <li role="none">
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onCopyRelativePath}>
          Copy Relative Path
        </button>
      </li>
      <li role="none">
        <button
          type="button"
          role="menuitem"
          className={styles.ctxMenuItem}
          onClick={onRevealInExplorer}
        >
          Reveal in Explorer
        </button>
      </li>
      <li role="none" aria-hidden="true" className={styles.ctxMenuSeparator} />
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
  filterLower: string;
  excludedPatterns: readonly string[];
  pendingCreate: { kind: 'file' | 'folder'; parentPath: string } | null;
  /** Bumped by "Collapse folders" — every node folds shut when it changes. */
  collapseSignal: number;
  onCommitCreate: (name: string) => Promise<void>;
  onCancelCreate: () => void;
  onOpenFile: (path: string) => Promise<void>;
  onContextMenu: (ev: MouseEvent, path: string, isDirectory: boolean) => void;
  onMoveInto: (sourcePath: string, targetDirectory: string) => Promise<void>;
};

const DRAG_MIME = 'application/x-cremniy-tree-path';

function FileTreeNode({
  workspaceRootPath,
  depth,
  entry,
  activeFilePath,
  filterLower,
  excludedPatterns,
  pendingCreate,
  collapseSignal,
  onCommitCreate,
  onCancelCreate,
  onOpenFile,
  onContextMenu,
  onMoveInto,
}: FileTreeNodeProps) {
  // NOTE: exclusion is filtered by the PARENT (entries.filter / children.filter)
  // — never with an early-return here, because the early return would sit
  // before the hooks below and break the Rules of Hooks (the original bug
  // that broke drag-drop entirely when any excluded pattern matched).
  const [expanded, setExpanded] = useState(false);
  const nameMatchesFilter =
    filterLower === '' || entry.name.toLowerCase().includes(filterLower);
  const [children, setChildren] = useState<WorkspaceDirectoryEntry[] | null>(null);
  const [childError, setChildError] = useState<string | null>(null);
  const [loadingChildren, setLoadingChildren] = useState(false);
  const [dragOver, setDragOver] = useState(false);

  const onDragStart = useCallback(
    (e: React.DragEvent<HTMLElement>) => {
      e.dataTransfer.effectAllowed = 'move';
      e.dataTransfer.setData(DRAG_MIME, entry.path);
      // text/plain fallback so OS shows a label.
      e.dataTransfer.setData('text/plain', entry.name);
    },
    [entry.name, entry.path],
  );

  const onDragOver = useCallback(
    (e: React.DragEvent<HTMLElement>) => {
      if (!entry.isDirectory) {
        return;
      }
      const sourcePath = e.dataTransfer.types.includes(DRAG_MIME)
        ? e.dataTransfer.getData(DRAG_MIME)
        : '';
      if (sourcePath === entry.path) {
        return;
      }
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
      setDragOver(true);
    },
    [entry.isDirectory, entry.path],
  );

  const onDragLeave = useCallback(() => setDragOver(false), []);

  const onDrop = useCallback(
    (e: React.DragEvent<HTMLElement>) => {
      setDragOver(false);
      if (!entry.isDirectory) {
        return;
      }
      const source = e.dataTransfer.getData(DRAG_MIME);
      if (source === '' || source === entry.path) {
        return;
      }
      e.preventDefault();
      // Stop the drop event from bubbling to an ancestor (nested <ul> or the
      // root tree). Without this, the same rename fires twice and the second
      // call errors with "from_path not found" — even though the file is
      // already in the new location.
      e.stopPropagation();
      void onMoveInto(source, entry.path);
    },
    [entry.isDirectory, entry.path, onMoveInto],
  );

  const paddingRem = 0.4 + depth * 0.85;

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

  // Auto-expand a directory while a filter is active so the user immediately
  // sees matches inside it. Only fires once per filter activation — the user
  // can still collapse it manually after.
  useEffect(() => {
    if (filterLower === '' || !entry.isDirectory || expanded) return;
    setExpanded(true);
    if (children == null) void loadChildren();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [filterLower]);

  // "Collapse folders" — fold shut when the signal changes. Skip the first run
  // (already collapsed) so it doesn't undo the filter auto-expand on mount.
  const collapseMountRef = useRef(true);
  useEffect(() => {
    if (collapseMountRef.current) {
      collapseMountRef.current = false;
      return;
    }
    setExpanded(false);
  }, [collapseSignal]);

  if (!entry.isDirectory) {
    if (!nameMatchesFilter) {
      return null;
    }
    const isActive = activeFilePath === entry.path;
    return (
      <button
        type="button"
        role="treeitem"
        aria-selected={isActive}
        className={`${styles.leafButton} ${isActive ? styles.leafButtonActive : ''}`}
        style={{ paddingLeft: `${paddingRem + 1.05}rem` }}
        draggable
        onDragStart={onDragStart}
        onClick={() => void onOpenFile(entry.path)}
        onContextMenu={(e) => onContextMenu(e, entry.path, false)}
        title={entry.name}
      >
        <FileIcon name={entry.name} />
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
        style={{
          paddingLeft: `${paddingRem}rem`,
          backgroundColor: dragOver ? 'rgba(255,255,255,0.08)' : undefined,
          outline: dragOver ? '1px dashed rgba(255,255,255,0.28)' : undefined,
        }}
        draggable
        onDragStart={onDragStart}
        onDragOver={onDragOver}
        onDragLeave={onDragLeave}
        onDrop={onDrop}
        onClick={() => toggleDir()}
        onContextMenu={(e) => onContextMenu(e, entry.path, true)}
        title={entry.name}
      >
        <ChevronIcon open={expanded} />
        <FolderIcon open={expanded} />
        <span className={styles.dirName}>{entry.name}</span>
        {loadingChildren ? <span className={styles.dirBusy}> …</span> : null}
      </button>
      {childError != null ? (
        <p className={styles.childError} role="alert" style={{ paddingLeft: `${paddingRem + 1}rem` }}>
          {childError}
        </p>
      ) : null}
      {expanded && children != null ? (
        <ul
          className={styles.nestedList}
          role="group"
          onDragOver={onDragOver}
          onDragLeave={onDragLeave}
          onDrop={onDrop}
        >
          {pendingCreate != null && pendingCreate.parentPath === entry.path ? (
            <li className={styles.treeItem} role="none">
              <InlineCreateRow
                kind={pendingCreate.kind}
                depth={depth + 1}
                onSubmit={(name) => void onCommitCreate(name)}
                onCancel={onCancelCreate}
              />
            </li>
          ) : null}
          {children
            .filter(
              (c) =>
                !excludedPatterns.some((p) =>
                  c.name.toLowerCase().includes(p.toLowerCase()),
                ),
            )
            .map((child) => (
              <li key={child.path} className={styles.treeItem} role="none">
                <FileTreeNode
                  workspaceRootPath={workspaceRootPath}
                  depth={depth + 1}
                  entry={child}
                  activeFilePath={activeFilePath}
                  filterLower={filterLower}
                  excludedPatterns={excludedPatterns}
                  pendingCreate={pendingCreate}
                  collapseSignal={collapseSignal}
                  onCommitCreate={onCommitCreate}
                  onCancelCreate={onCancelCreate}
                  onOpenFile={onOpenFile}
                  onContextMenu={onContextMenu}
                  onMoveInto={onMoveInto}
                />
              </li>
            ))}
        </ul>
      ) : null}
    </div>
  );
}

/**
 * Inline-create row — replaces window.prompt with a text input that lives
 * inside the tree at the position the new entry will appear. Enter commits,
 * Escape cancels, blur cancels (so clicking away in the tree closes the row
 * without creating a stray empty file). Auto-focus on mount.
 */
function InlineCreateRow({
  kind,
  depth,
  onSubmit,
  onCancel,
}: {
  kind: 'file' | 'folder';
  depth: number;
  onSubmit: (name: string) => void;
  onCancel: () => void;
}) {
  const [value, setValue] = useState('');
  const paddingRem = 0.4 + depth * 0.85 + 1.05;
  const icon = kind === 'folder' ? '📁' : '📄';
  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '0.35rem',
        paddingLeft: `${paddingRem}rem`,
        paddingRight: '0.5rem',
        height: '1.6rem',
      }}
    >
      <span aria-hidden style={{ opacity: 0.7, fontSize: 12 }}>
        {icon}
      </span>
      <input
        autoFocus
        type="text"
        value={value}
        onChange={(e) => setValue(e.currentTarget.value)}
        onKeyDown={(e) => {
          if (e.key === 'Enter') {
            e.preventDefault();
            onSubmit(value);
          } else if (e.key === 'Escape') {
            e.preventDefault();
            onCancel();
          }
        }}
        onBlur={() => {
          // Defer one tick so a sibling click on a button still gets its
          // mousedown/up before we tear the row down.
          window.setTimeout(() => onCancel(), 0);
        }}
        placeholder={kind === 'folder' ? 'folder name' : 'file name'}
        style={{
          flex: 1,
          minWidth: 0,
          padding: '0.1rem 0.35rem',
          fontSize: 12,
          fontFamily: 'inherit',
          color: 'inherit',
          background: 'rgba(255,255,255,0.05)',
          border: '1px solid rgba(255,255,255,0.28)',
          borderRadius: 3,
          outline: 'none',
        }}
      />
    </div>
  );
}
