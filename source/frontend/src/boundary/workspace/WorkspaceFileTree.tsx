import { createContext, useCallback, useContext, useEffect, useMemo, useRef, useState } from 'react';
import type { CSSProperties, LegacyRef, MouseEvent } from 'react';

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
import {
  loadManualNesting,
  loadTreeOrder,
  saveManualNesting,
  saveTreeOrder,
  sortByOrder,
  type ManualNesting,
  type TreeOrder,
} from './treeView';
import { DEFAULT_NESTING_PATTERNS, computeNesting, type NestingResult } from './nesting';

import styles from './WorkspaceFileTree.module.css';

type WorkspaceFileTreeProps = {
  workspaceRoot: WorkspaceRoot | null;
  /** When set, the tree filters to entries whose name contains this (the Search
   *  view drives it). Empty / undefined → show everything (Explorer). */
  filter?: string;
};

type CtxState = {
  clientX: number;
  clientY: number;
  path: string;
  isDirectory: boolean;
  /** True when the right-clicked row is a nested child → offer "Un-nest". */
  isNested: boolean;
};

function formatErr(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

/** Central nav state for the recursive tree: which paths are expanded, which row
 *  owns the roving tab stop, and the controls to change them. Provided once at
 *  the root so nodes don't prop-drill it and keyboard handling can drive any
 *  row. (Also the seam a future flat/virtualized renderer plugs into.) */
type TreeNav = {
  expandedPaths: Set<string>;
  focusedPath: string | null;
  toggleExpand: (path: string) => void;
  expandPath: (path: string) => void;
  setFocusedPath: (path: string) => void;
  // Refresh tick: bumped on focus/poll/explicit refresh. Expanded directory
  // nodes re-read their children whenever this changes.
  revision: number;
};
const TreeNavContext = createContext<TreeNav | null>(null);
function useTreeNav(): TreeNav {
  const v = useContext(TreeNavContext);
  if (v == null) {
    throw new Error('useTreeNav must be used within the workspace file tree');
  }
  return v;
}

/** One directory level → its visible roots + auto-nested children. Custom order
 *  is applied first; nesting is skipped while a filter is active so search stays
 *  flat and complete. The caller passes entries already filtered by exclusions. */
function resolveLevel(
  entries: WorkspaceDirectoryEntry[],
  dirPath: string,
  treeOrder: TreeOrder,
  filterLower: string,
  manual: ManualNesting,
): NestingResult {
  const sorted = sortByOrder(entries, dirPath, treeOrder);
  if (filterLower !== '') {
    return { roots: sorted, childrenOf: new Map() };
  }
  return computeNesting(sorted, DEFAULT_NESTING_PATTERNS, manual[dirPath] ?? {});
}

export function WorkspaceFileTree({ workspaceRoot, filter }: WorkspaceFileTreeProps) {
  const { activeFilePath, openFileFromWorkspace, fileTreeRevision, bumpFileTreeRevision } = useIdeSession();
  const notify = useNotify();
  const [entries, setEntries] = useState<WorkspaceDirectoryEntry[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const [ctx, setCtx] = useState<CtxState | null>(null);
  const [excludedPatterns, setExcludedPatterns] = useState<readonly string[]>([]);
  // Per-directory custom display order — purely visual, persisted per workspace
  // (localStorage v1). Default is the backend's folders-first alphabetical.
  const [treeOrder, setTreeOrder] = useState<TreeOrder>({});
  // Manual file-nesting overrides (drag file→file to nest, right-click to
  // un-nest). Layered over the automatic patterns; persisted per workspace.
  const [manualNesting, setManualNesting] = useState<ManualNesting>({});
  useEffect(() => {
    setTreeOrder(loadTreeOrder(workspaceRoot?.path ?? ''));
    setManualNesting(loadManualNesting(workspaceRoot?.path ?? ''));
  }, [workspaceRoot?.path]);
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
  // Expansion is owned centrally (a Set of expanded paths), not per-node, so
  // keyboard navigation can expand/collapse any row, "Collapse folders" is one
  // set-clear, and we have the flat model a future virtualization needs.
  const [expandedPaths, setExpandedPaths] = useState<Set<string>>(() => new Set());
  // Roving-tabindex focus: path of the row that currently owns the tab stop.
  const [focusedPath, setFocusedPath] = useState<string | null>(null);
  const treeRootRef = useRef<HTMLDivElement | null>(null);
  const ctxMenuRef = useRef<HTMLUListElement | null>(null);
  // Driven by the Search view; empty when shown as the Explorer.
  const filterLower = (filter ?? '').trim().toLowerCase();

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

  const openCtx = useCallback(
    (ev: MouseEvent, path: string, isDirectory: boolean, isNested = false) => {
      ev.preventDefault();
      ev.stopPropagation();
      setCtx({ clientX: ev.clientX, clientY: ev.clientY, path, isDirectory, isNested });
    },
    [],
  );

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
  const toggleExpand = useCallback((path: string) => {
    setExpandedPaths((prev) => {
      const next = new Set(prev);
      if (next.has(path)) next.delete(path);
      else next.add(path);
      return next;
    });
  }, []);
  const expandPath = useCallback((path: string) => {
    setExpandedPaths((prev) => (prev.has(path) ? prev : new Set(prev).add(path)));
  }, []);
  const collapsePath = useCallback((path: string) => {
    setExpandedPaths((prev) => {
      if (!prev.has(path)) return prev;
      const next = new Set(prev);
      next.delete(path);
      return next;
    });
  }, []);
  const collapseAll = useCallback(() => {
    setExpandedPaths(new Set());
    setFocusedPath(null);
  }, []);

  // Keyboard navigation for the whole tree (WAI-ARIA tree pattern). Rows are the
  // [role=treeitem] elements; focus moves through them in DOM (== visual) order
  // and expand/collapse drives the central Set. Enter/Space stay native on the
  // row buttons — we own only movement + expand/collapse.
  const onTreeKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLDivElement>) => {
      const root = treeRootRef.current;
      if (root == null) return;
      const items = Array.from(root.querySelectorAll<HTMLElement>('[role="treeitem"]'));
      if (items.length === 0) return;
      const active = document.activeElement as HTMLElement | null;
      const idx = active != null ? items.indexOf(active) : -1;
      const focusAt = (i: number) => {
        const el = items[Math.max(0, Math.min(items.length - 1, i))];
        if (el == null) return;
        const p = el.getAttribute('data-path');
        if (p != null) setFocusedPath(p);
        el.focus();
      };
      switch (e.key) {
        case 'ArrowDown':
          e.preventDefault();
          focusAt(idx + 1);
          break;
        case 'ArrowUp':
          e.preventDefault();
          focusAt(idx - 1);
          break;
        case 'Home':
          e.preventDefault();
          focusAt(0);
          break;
        case 'End':
          e.preventDefault();
          focusAt(items.length - 1);
          break;
        case 'ArrowRight': {
          if (idx < 0) break;
          e.preventDefault();
          const el = items[idx]!;
          const path = el.getAttribute('data-path');
          const expandable = el.getAttribute('data-expandable') === 'true';
          const isExpanded = el.getAttribute('data-expanded') === 'true';
          if (expandable && path != null && !isExpanded) expandPath(path);
          else if (expandable && isExpanded) focusAt(idx + 1);
          break;
        }
        case 'ArrowLeft': {
          if (idx < 0) break;
          e.preventDefault();
          const el = items[idx]!;
          const path = el.getAttribute('data-path');
          const expandable = el.getAttribute('data-expandable') === 'true';
          const isExpanded = el.getAttribute('data-expanded') === 'true';
          if (expandable && isExpanded && path != null) {
            collapsePath(path);
          } else {
            const depth = Number(el.getAttribute('data-depth') ?? '0');
            for (let i = idx - 1; i >= 0; i--) {
              if (Number(items[i]!.getAttribute('data-depth') ?? '0') < depth) {
                focusAt(i);
                break;
              }
            }
          }
          break;
        }
        default:
          break;
      }
    },
    [expandPath, collapsePath],
  );

  const treeNav = useMemo<TreeNav>(
    () => ({
      expandedPaths,
      focusedPath,
      toggleExpand,
      expandPath,
      setFocusedPath,
      revision: fileTreeRevision,
    }),
    [expandedPaths, focusedPath, toggleExpand, expandPath, fileTreeRevision],
  );

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

  // Custom sort: swap the right-clicked entry with its visible neighbour in the
  // same directory and persist the new order. Re-reads the parent dir so it
  // works the same at the root and inside any (lazily-loaded) folder.
  const runMoveEntry = useCallback(
    async (direction: 'up' | 'down') => {
      if (ctx == null || workspaceRoot == null || ctx.path === workspaceRoot.path) {
        return;
      }
      const dirPath = parentDirectoryPath(ctx.path);
      const name = fileNameFromPath(ctx.path);
      try {
        const siblings = await listDirectoryEntries(workspaceRoot.path, dirPath);
        const visible = siblings.filter(
          (e) => !excludedPatterns.some((p) => e.name.toLowerCase().includes(p.toLowerCase())),
        );
        const ordered = sortByOrder(visible, dirPath, treeOrder).map((e) => e.name);
        const idx = ordered.indexOf(name);
        const swapWith = direction === 'up' ? idx - 1 : idx + 1;
        if (idx < 0 || swapWith < 0 || swapWith >= ordered.length) {
          setCtx(null);
          return;
        }
        const a = ordered[idx]!;
        ordered[idx] = ordered[swapWith]!;
        ordered[swapWith] = a;
        setTreeOrder((prev) => {
          const next = { ...prev, [dirPath]: ordered };
          saveTreeOrder(workspaceRoot.path, next);
          return next;
        });
        setCtx(null);
      } catch (e) {
        notify.error('Operation failed', formatErr(e));
      }
    },
    [ctx, workspaceRoot, excludedPatterns, treeOrder, notify],
  );

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

  // Manual nest: drag a sibling file onto another file → record the override.
  // Visual only — nothing moves on disk. Siblings only; cross-dir drops are
  // handled by the folder move path instead.
  const handleNestUnder = useCallback(
    (sourcePath: string, targetPath: string) => {
      if (workspaceRoot == null || sourcePath === targetPath) {
        return;
      }
      const dir = parentDirectoryPath(targetPath);
      if (parentDirectoryPath(sourcePath) !== dir) {
        return;
      }
      const sourceName = fileNameFromPath(sourcePath);
      const targetName = fileNameFromPath(targetPath);
      if (sourceName === '' || targetName === '' || sourceName === targetName) {
        return;
      }
      setManualNesting((prev) => {
        const next: ManualNesting = {
          ...prev,
          [dir]: { ...prev[dir], [sourceName]: targetName },
        };
        saveManualNesting(workspaceRoot.path, next);
        return next;
      });
    },
    [workspaceRoot],
  );

  // Un-nest the right-clicked file: force it back to the top level (null also
  // overrides any automatic pattern that would re-nest it).
  const runUnnest = useCallback(() => {
    if (ctx == null || workspaceRoot == null) {
      return;
    }
    const dir = parentDirectoryPath(ctx.path);
    const name = fileNameFromPath(ctx.path);
    setManualNesting((prev) => {
      const next: ManualNesting = { ...prev, [dir]: { ...prev[dir], [name]: null } };
      saveManualNesting(workspaceRoot.path, next);
      return next;
    });
    setCtx(null);
  }, [ctx, workspaceRoot]);

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
            onMoveUp={() => void runMoveEntry('up')}
            onMoveDown={() => void runMoveEntry('down')}
            showUnnest={ctx.isNested}
            onUnnest={() => runUnnest()}
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
  const rootLevel = resolveLevel(
    visibleEntries,
    workspaceRoot.path,
    treeOrder,
    filterLower,
    manualNesting,
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
    <TreeNavContext.Provider value={treeNav}>
      <div className={styles.section}>
        <TreeHeader
          name={fileNameFromPath(workspaceRoot.path) || workspaceRoot.path}
          onNewFile={newFileAtRoot}
          onNewFolder={newFolderAtRoot}
          onRefresh={bumpFileTreeRevision}
          onCollapseAll={collapseAll}
        />
        <div
          ref={treeRootRef}
          className={styles.treeRoot}
          role="tree"
          aria-label="Workspace files"
          tabIndex={-1}
          onKeyDown={onTreeKeyDown}
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
          {rootLevel.roots.map((entry, index) => (
            <li key={entry.path} className={styles.treeItem} role="none">
              <FileTreeNode
                workspaceRootPath={workspaceRoot.path}
                depth={0}
                entry={entry}
                activeFilePath={activeFilePath}
                filterLower={filterLower}
                excludedPatterns={excludedPatterns}
                treeOrder={treeOrder}
                manualNesting={manualNesting}
                nestedChildren={rootLevel.childrenOf.get(entry.path)}
                pendingCreate={pendingCreate}
                isFirstRow={index === 0}
                onCommitCreate={commitPendingCreate}
                onCancelCreate={cancelPendingCreate}
                onOpenFile={openFileFromWorkspace}
                onContextMenu={openCtx}
                onMoveInto={handleMoveInto}
                onNestUnder={handleNestUnder}
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
          onMoveUp={() => void runMoveEntry('up')}
          onMoveDown={() => void runMoveEntry('down')}
          showUnnest={ctx.isNested}
          onUnnest={() => runUnnest()}
          onRename={() => void runRename()}
          onDelete={() => void runDelete()}
        />
      ) : null}
    </TreeNavContext.Provider>
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
          <svg aria-hidden width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M14 3H6.5A1.5 1.5 0 0 0 5 4.5v15A1.5 1.5 0 0 0 6.5 21h11a1.5 1.5 0 0 0 1.5-1.5V8z" />
            <path d="M14 3v5h5" />
            <path d="M12 11.5v5M9.5 14h5" />
          </svg>
        </button>
        <button type="button" className={styles.headerBtn} title="New folder" aria-label="New folder" onClick={onNewFolder}>
          <svg aria-hidden width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M3.5 6.5A1.5 1.5 0 0 1 5 5h3.8l2 2H19a1.5 1.5 0 0 1 1.5 1.5V17A1.5 1.5 0 0 1 19 18.5H5A1.5 1.5 0 0 1 3.5 17z" />
            <path d="M12 10.5v5M9.5 13h5" />
          </svg>
        </button>
        <button type="button" className={styles.headerBtn} title="Refresh explorer" aria-label="Refresh explorer" onClick={onRefresh}>
          <svg aria-hidden width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M20.5 12a8.5 8.5 0 1 1-2.5-6" />
            <path d="M20.5 3.5V9H15" />
          </svg>
        </button>
        <button type="button" className={styles.headerBtn} title="Collapse folders" aria-label="Collapse folders" onClick={onCollapseAll}>
          <svg aria-hidden width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M7 11l5-5 5 5" />
            <path d="M7 17l5-5 5 5" />
          </svg>
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
  onMoveUp: () => void;
  onMoveDown: () => void;
  showUnnest: boolean;
  onUnnest: () => void;
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
  onMoveUp,
  onMoveDown,
  showUnnest,
  onUnnest,
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
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onMoveUp}>
          Move up
        </button>
      </li>
      <li role="none">
        <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onMoveDown}>
          Move down
        </button>
      </li>
      {showUnnest ? (
        <li role="none">
          <button type="button" role="menuitem" className={styles.ctxMenuItem} onClick={onUnnest}>
            Un-nest
          </button>
        </li>
      ) : null}
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
  treeOrder: TreeOrder;
  manualNesting: ManualNesting;
  /** Files auto-nested under THIS entry (only set for a file that owns nests). */
  nestedChildren?: WorkspaceDirectoryEntry[];
  /** True when this row is rendered as a nested child (enables "Un-nest"). */
  isNested?: boolean;
  pendingCreate: { kind: 'file' | 'folder'; parentPath: string } | null;
  /** Only the very first root row sets this, to seed the roving tab stop. */
  isFirstRow?: boolean;
  onCommitCreate: (name: string) => Promise<void>;
  onCancelCreate: () => void;
  onOpenFile: (path: string) => Promise<void>;
  onContextMenu: (ev: MouseEvent, path: string, isDirectory: boolean, isNested?: boolean) => void;
  onMoveInto: (sourcePath: string, targetDirectory: string) => Promise<void>;
  /** Drag a sibling file onto another file → nest it under that file. */
  onNestUnder: (sourcePath: string, targetPath: string) => void;
};

const DRAG_MIME = 'application/x-cremniy-tree-path';
// Present on the drag payload only when the dragged item is a FILE — lets a file
// row light up as a nest target (dragover can read `types` but not the data).
const DRAG_FILE_MIME = 'application/x-cremniy-tree-file';

function FileTreeNode({
  workspaceRootPath,
  depth,
  entry,
  activeFilePath,
  filterLower,
  excludedPatterns,
  treeOrder,
  manualNesting,
  nestedChildren,
  isNested,
  pendingCreate,
  isFirstRow,
  onCommitCreate,
  onCancelCreate,
  onOpenFile,
  onContextMenu,
  onMoveInto,
  onNestUnder,
}: FileTreeNodeProps) {
  // NOTE: exclusion is filtered by the PARENT (entries.filter / children.filter)
  // — never with an early-return here, because the early return would sit
  // before the hooks below and break the Rules of Hooks (the original bug
  // that broke drag-drop entirely when any excluded pattern matched).
  const nav = useTreeNav();
  const expanded = nav.expandedPaths.has(entry.path);
  const isTabStop =
    nav.focusedPath === entry.path || (nav.focusedPath == null && isFirstRow === true);
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
      if (!entry.isDirectory) {
        e.dataTransfer.setData(DRAG_FILE_MIME, '1');
      }
      // text/plain fallback so OS shows a label.
      e.dataTransfer.setData('text/plain', entry.name);
    },
    [entry.isDirectory, entry.name, entry.path],
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

  // A file row accepts a sibling FILE drop → nest it under this file (visual).
  const onNestDragOver = useCallback(
    (e: React.DragEvent<HTMLElement>) => {
      if (entry.isDirectory || !e.dataTransfer.types.includes(DRAG_FILE_MIME)) {
        return;
      }
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
      setDragOver(true);
    },
    [entry.isDirectory],
  );

  const onNestDrop = useCallback(
    (e: React.DragEvent<HTMLElement>) => {
      setDragOver(false);
      if (entry.isDirectory || !e.dataTransfer.types.includes(DRAG_FILE_MIME)) {
        return;
      }
      const source = e.dataTransfer.getData(DRAG_MIME);
      if (source === '' || source === entry.path) {
        return;
      }
      e.preventDefault();
      e.stopPropagation();
      onNestUnder(source, entry.path);
    },
    [entry.isDirectory, entry.path, onNestUnder],
  );

  const paddingRem = 0.4 + depth * 0.85;

  const loadChildren = useCallback(
    async (silent = false) => {
      // Silent reloads (auto-refresh) keep the current rows on screen and just
      // swap in fresh data — no spinner flash. The first load shows the spinner.
      if (!silent) {
        setLoadingChildren(true);
      }
      setChildError(null);
      try {
        const list = await listDirectoryEntries(workspaceRootPath, entry.path);
        setChildren(list);
      } catch (e: unknown) {
        setChildError(e instanceof Error ? e.message : String(e));
      } finally {
        if (!silent) {
          setLoadingChildren(false);
        }
      }
    },
    [workspaceRootPath, entry.path],
  );

  const toggleDir = useCallback(() => {
    if (!entry.isDirectory) {
      return;
    }
    nav.toggleExpand(entry.path);
  }, [entry.isDirectory, entry.path, nav.toggleExpand]);

  // Load children when this directory is expanded, and re-read on every refresh
  // tick (nav.revision — bumped by focus/poll/explicit refresh) so an expanded
  // folder stays in sync with disk without a manual Refresh. The first load
  // shows a spinner; later refreshes swap rows in silently.
  useEffect(() => {
    if (entry.isDirectory && expanded) {
      void loadChildren(children != null);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [expanded, entry.isDirectory, nav.revision]);

  // Auto-expand a directory while a filter is active so matches inside are
  // immediately visible. Expansion is central now, so we just request it.
  useEffect(() => {
    if (filterLower !== '' && entry.isDirectory && !expanded) {
      nav.expandPath(entry.path);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [filterLower]);

  if (!entry.isDirectory) {
    if (!nameMatchesFilter) {
      return null;
    }
    const isActive = activeFilePath === entry.path;
    const nested = nestedChildren ?? [];
    if (nested.length === 0) {
      return (
        <button
          type="button"
          role="treeitem"
          aria-selected={isActive}
          className={`${styles.leafButton} ${isActive ? styles.leafButtonActive : ''}`}
          style={{
            paddingLeft: `${paddingRem + 1.05}rem`,
            outline: dragOver ? '1px dashed rgba(255,255,255,0.28)' : undefined,
            backgroundColor: dragOver ? 'rgba(255,255,255,0.06)' : undefined,
          }}
          tabIndex={isTabStop ? 0 : -1}
          data-path={entry.path}
          data-depth={depth}
          data-expandable="false"
          draggable
          onDragStart={onDragStart}
          onDragOver={onNestDragOver}
          onDragLeave={onDragLeave}
          onDrop={onNestDrop}
          onFocus={() => nav.setFocusedPath(entry.path)}
          onClick={() => void onOpenFile(entry.path)}
          onContextMenu={(e) => onContextMenu(e, entry.path, false, isNested)}
          title={entry.name}
        >
          <FileIcon name={entry.name} />
          <span className={styles.leafName}>{entry.name}</span>
        </button>
      );
    }
    // A file that owns auto-nested children: the row opens the file, the twistie
    // expands its nested siblings. One level deep — nested files are plain leaves.
    return (
      <div className={styles.dirBlock}>
        <div
          role="treeitem"
          aria-expanded={expanded}
          aria-selected={isActive}
          className={`${styles.leafButton} ${isActive ? styles.leafButtonActive : ''}`}
          style={{
            paddingLeft: `${paddingRem}rem`,
            outline: dragOver ? '1px dashed rgba(255,255,255,0.28)' : undefined,
            backgroundColor: dragOver ? 'rgba(255,255,255,0.06)' : undefined,
          }}
          tabIndex={isTabStop ? 0 : -1}
          data-path={entry.path}
          data-depth={depth}
          data-expandable="true"
          data-expanded={expanded}
          draggable
          onDragStart={onDragStart}
          onDragOver={onNestDragOver}
          onDragLeave={onDragLeave}
          onDrop={onNestDrop}
          onFocus={() => nav.setFocusedPath(entry.path)}
          onClick={() => void onOpenFile(entry.path)}
          onKeyDown={(e) => {
            if (e.key === 'Enter') void onOpenFile(entry.path);
          }}
          onContextMenu={(e) => onContextMenu(e, entry.path, false, isNested)}
          title={entry.name}
        >
          <span
            className={styles.nestTwistie}
            aria-hidden="true"
            onClick={(e) => {
              e.stopPropagation();
              nav.toggleExpand(entry.path);
            }}
          >
            <ChevronIcon open={expanded} />
          </span>
          <FileIcon name={entry.name} />
          <span className={styles.leafName}>{entry.name}</span>
        </div>
        {expanded ? (
          <ul
            className={styles.nestedList}
            role="group"
            style={{ ['--indent']: `${paddingRem}rem` } as CSSProperties}
          >
            {nested.map((child) => (
              <li key={child.path} className={styles.treeItem} role="none">
                <FileTreeNode
                  workspaceRootPath={workspaceRootPath}
                  depth={depth + 1}
                  entry={child}
                  activeFilePath={activeFilePath}
                  filterLower={filterLower}
                  excludedPatterns={excludedPatterns}
                  treeOrder={treeOrder}
                  manualNesting={manualNesting}
                  isNested
                  pendingCreate={pendingCreate}
                  onCommitCreate={onCommitCreate}
                  onCancelCreate={onCancelCreate}
                  onOpenFile={onOpenFile}
                  onContextMenu={onContextMenu}
                  onMoveInto={onMoveInto}
                  onNestUnder={onNestUnder}
                />
              </li>
            ))}
          </ul>
        ) : null}
      </div>
    );
  }

  const childLevel = resolveLevel(
    (children ?? []).filter(
      (c) => !excludedPatterns.some((p) => c.name.toLowerCase().includes(p.toLowerCase())),
    ),
    entry.path,
    treeOrder,
    filterLower,
    manualNesting,
  );

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
        tabIndex={isTabStop ? 0 : -1}
        data-path={entry.path}
        data-depth={depth}
        data-expandable="true"
        data-expanded={expanded}
        draggable
        onDragStart={onDragStart}
        onDragOver={onDragOver}
        onDragLeave={onDragLeave}
        onDrop={onDrop}
        onFocus={() => nav.setFocusedPath(entry.path)}
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
          style={{ ['--indent']: `${paddingRem}rem` } as CSSProperties}
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
          {childLevel.roots.map((child) => (
              <li key={child.path} className={styles.treeItem} role="none">
                <FileTreeNode
                  workspaceRootPath={workspaceRootPath}
                  depth={depth + 1}
                  entry={child}
                  activeFilePath={activeFilePath}
                  filterLower={filterLower}
                  excludedPatterns={excludedPatterns}
                  treeOrder={treeOrder}
                  manualNesting={manualNesting}
                  nestedChildren={childLevel.childrenOf.get(child.path)}
                  pendingCreate={pendingCreate}
                  onCommitCreate={onCommitCreate}
                  onCancelCreate={onCancelCreate}
                  onOpenFile={onOpenFile}
                  onContextMenu={onContextMenu}
                  onMoveInto={onMoveInto}
                  onNestUnder={onNestUnder}
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
      {kind === 'folder' ? <FolderIcon /> : <FileIcon name={value} />}
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
