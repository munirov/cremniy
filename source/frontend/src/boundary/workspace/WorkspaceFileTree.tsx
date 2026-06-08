import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
} from 'react';
import type { LegacyRef, MouseEvent } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';
import type { WorkspaceDirectoryEntry } from '@domain/workspace/directoryEntry';
import {
  fileNameFromPath,
  joinFilePath,
  normalizeFsPath,
  parentDirectoryPath,
} from '@domain/workspace/paths';
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
import { GitDecorationsProvider, useGitDecoration } from './GitDecorationsContext';
import type { GitDecoKind, GitDecoResult } from './gitDecorations';
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

/** Indent step per depth level (rem). The leaf glyph column lines up under a
 *  parent folder's name. Kept as a constant so the flat row renderer, the
 *  inline-create row and the indent guides all agree. */
const INDENT_REM = 0.85;
const BASE_PAD_REM = 0.4;
/** Extra left pad for a leaf with no twistie, so its icon aligns with the
 *  folder icon of sibling directories (which spend that column on the chevron). */
const LEAF_GLYPH_REM = 1.05;

function rowPadRem(depth: number): number {
  return BASE_PAD_REM + depth * INDENT_REM;
}

/** Folder paths from the workspace root (exclusive) down to `filePath`'s parent
 *  (inclusive), in root→down order — the directories the tree must expand so
 *  `filePath`'s row becomes visible ("reveal active file"). The root itself is
 *  omitted (its children are the top-level rows, so it's never an expandable
 *  row). Returns [] when the file isn't under the root. Handles `/` and `\\`;
 *  the climb compares via normalizeFsPath so Windows drive/UNC + case
 *  differences don't break the match. */
function ancestorFoldersToReveal(filePath: string, rootPath: string): string[] {
  if (filePath === '' || rootPath === '') return [];
  const rootKey = normalizeFsPath(rootPath);
  const chain: string[] = [];
  let dir = parentDirectoryPath(filePath);
  while (dir !== '' && normalizeFsPath(dir) !== rootKey) {
    chain.push(dir);
    const parent = parentDirectoryPath(dir);
    if (parent === dir) return []; // no upward progress — bail rather than loop
    dir = parent;
  }
  // Valid only if the climb landed exactly on the root; otherwise the file lived
  // outside the workspace and the partial chain is discarded.
  if (normalizeFsPath(dir) !== rootKey) return [];
  return chain.reverse();
}

/** Fallback row height (px) used for windowing math until a real row is measured.
 *  Matches the CSS: 13px * 1.5 line-height + 2px*2 padding ≈ 23.5. */
const DEFAULT_ROW_H = 24;
/** Rows rendered above/below the viewport so fast scrolls don't show blank gaps. */
const OVERSCAN = 8;

/** Central nav state for the tree: which paths are expanded, which row owns the
 *  roving tab stop, and the controls to change them. Provided once at the root so
 *  the (flat) row renderer doesn't prop-drill it and keyboard handling can drive
 *  any row by index. */
type TreeNav = {
  expandedPaths: Set<string>;
  focusedPath: string | null;
  toggleExpand: (path: string) => void;
  expandPath: (path: string) => void;
  setFocusedPath: (path: string) => void;
  // Refresh tick: bumped on focus/poll/explicit refresh. Drives the central
  // re-read of every expanded directory's children.
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

/** A single visible row in the flattened (windowed) tree. The recursive
 *  FileTreeNode used to compute these on the fly; the flat model centralizes
 *  them so we can render only the slice that's on screen. */
type FlatRow =
  | {
      kind: 'entry';
      /** Stable key + identity. */
      path: string;
      entry: WorkspaceDirectoryEntry;
      depth: number;
      /** True when rendered as a nested child (enables "Un-nest"). */
      isNested: boolean;
      /** Directory, or a file that owns auto-nested children → has a twistie. */
      expandable: boolean;
      expanded: boolean;
      /** Files auto-nested directly under this row (only for a nesting file). */
      nestedChildren?: WorkspaceDirectoryEntry[];
    }
  | {
      // Inline create input, rendered as the first child of its parent dir.
      kind: 'create';
      path: string; // synthetic, for React key
      depth: number;
      createKind: 'file' | 'folder';
    }
  | {
      // A directory whose child listing failed.
      kind: 'error';
      path: string; // synthetic key
      depth: number;
      message: string;
    };

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
  // bug we fixed in FileTreeNode. This drives the root-zone drop-target.
  const [rootDragOver, setRootDragOver] = useState(false);
  // Expansion is owned centrally (a Set of expanded paths) so keyboard nav can
  // expand/collapse any row, "Collapse folders" is one set-clear, and the flat
  // (virtualized) renderer below has the model it needs.
  const [expandedPaths, setExpandedPaths] = useState<Set<string>>(() => new Set());
  // Roving-tabindex focus: path of the row that currently owns the tab stop.
  const [focusedPath, setFocusedPath] = useState<string | null>(null);
  // "Reveal active file" target: set when activeFilePath changes, cleared once
  // its row is found + scrolled into view (or the chain is fully loaded without
  // it). Kept separate from focusedPath because the row may not exist yet while
  // its ancestor folders are still loading lazily.
  const [pendingReveal, setPendingReveal] = useState<string | null>(null);

  // Centralized lazy child-loading. The recursive nodes used to each own their
  // children; the flat model needs one shared cache. `childCache` is dirPath →
  // its entries; `childLoading`/`childErrors` track per-dir spinner + failure.
  const [childCache, setChildCache] = useState<Map<string, WorkspaceDirectoryEntry[]>>(
    () => new Map(),
  );
  const [childLoading, setChildLoading] = useState<Set<string>>(() => new Set());
  const [childErrors, setChildErrors] = useState<Map<string, string>>(() => new Map());

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

  // Reset the child cache whenever the workspace itself changes (paths from a
  // different root are meaningless here). Refresh ticks DON'T clear it — they
  // re-read in place (silent), handled in the loader effect below.
  useEffect(() => {
    setChildCache(new Map());
    setChildLoading(new Set());
    setChildErrors(new Map());
    setExpandedPaths(new Set());
    setFocusedPath(null);
  }, [workspaceRoot?.path]);

  // Tracks which workspace's root we've already loaded. A refresh tick reuses it
  // (silent reload); only a genuine workspace change does the full clear+spinner.
  const loadedRootRef = useRef<string | null>(null);
  useEffect(() => {
    if (workspaceRoot == null || workspaceRoot.path === '') {
      loadedRootRef.current = null;
      setEntries(null);
      setError(null);
      setLoading(false);
      return;
    }

    // Initial load (new workspace) shows the spinner and clears stale rows.
    // A refresh tick (focus/poll) reloads silently — no null flash, no spinner,
    // no scroll jump — so the tree doesn't twitch every few seconds.
    const isInitial = loadedRootRef.current !== workspaceRoot.path;
    loadedRootRef.current = workspaceRoot.path;

    let cancelled = false;
    if (isInitial) {
      setLoading(true);
      setError(null);
      setEntries(null);
    }

    void listDirectoryEntries(workspaceRoot.path, workspaceRoot.path)
      .then((list) => {
        if (!cancelled) {
          setEntries(list);
          setError(null);
        }
      })
      .catch((e: unknown) => {
        if (!cancelled && isInitial) {
          setError(e instanceof Error ? e.message : String(e));
        }
      })
      .finally(() => {
        if (!cancelled && isInitial) {
          setLoading(false);
        }
      });

    return () => {
      cancelled = true;
    };
  }, [workspaceRoot, fileTreeRevision]);

  const rootPath = workspaceRoot?.path ?? '';

  // Centralized directory child-loader. Loads (or silently re-reads) the
  // children of one directory and folds the result into the shared caches.
  // `silent` keeps the current rows on screen during auto-refresh; the first
  // load flips the per-dir spinner.
  const loadDir = useCallback(
    async (dirPath: string, silent: boolean) => {
      if (rootPath === '') return;
      if (!silent) {
        setChildLoading((prev) => {
          if (prev.has(dirPath)) return prev;
          const next = new Set(prev);
          next.add(dirPath);
          return next;
        });
      }
      setChildErrors((prev) => {
        if (!prev.has(dirPath)) return prev;
        const next = new Map(prev);
        next.delete(dirPath);
        return next;
      });
      try {
        const list = await listDirectoryEntries(rootPath, dirPath);
        setChildCache((prev) => {
          const next = new Map(prev);
          next.set(dirPath, list);
          return next;
        });
      } catch (e: unknown) {
        setChildErrors((prev) => {
          const next = new Map(prev);
          next.set(dirPath, e instanceof Error ? e.message : String(e));
          return next;
        });
      } finally {
        if (!silent) {
          setChildLoading((prev) => {
            if (!prev.has(dirPath)) return prev;
            const next = new Set(prev);
            next.delete(dirPath);
            return next;
          });
        }
      }
    },
    [rootPath],
  );

  // Load children for every expanded directory that hasn't been loaded yet
  // (first expand → spinner). This replaces the per-node load effect.
  useEffect(() => {
    if (rootPath === '') return;
    for (const dirPath of expandedPaths) {
      if (!childCache.has(dirPath) && !childLoading.has(dirPath)) {
        void loadDir(dirPath, false);
      }
    }
  }, [expandedPaths, childCache, childLoading, loadDir, rootPath]);

  // Re-expanding an already-loaded folder silently refreshes it (matches the old
  // per-node behaviour: collapse kept the data, re-expand re-read it without a
  // spinner). Detected by diffing against the previous expanded set so it fires
  // only on the expand transition — never on a cache change, so no loop.
  const prevExpandedRef = useRef<Set<string>>(expandedPaths);
  useEffect(() => {
    const prev = prevExpandedRef.current;
    prevExpandedRef.current = expandedPaths;
    if (rootPath === '') return;
    for (const dirPath of expandedPaths) {
      if (!prev.has(dirPath) && childCache.has(dirPath)) {
        void loadDir(dirPath, true);
      }
    }
    // childCache read fresh; gating on the expand transition (prev set) is what
    // keeps this from looping when the cache updates.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [expandedPaths, rootPath, loadDir]);

  // Silent re-read of all already-loaded + expanded directories on each refresh
  // tick (focus/poll/explicit refresh) so open folders stay in sync with disk
  // without a manual Refresh and without a spinner flash. Mirrors the root
  // listing's silent-reload semantics.
  const lastRevisionRef = useRef(fileTreeRevision);
  useEffect(() => {
    if (lastRevisionRef.current === fileTreeRevision) return;
    lastRevisionRef.current = fileTreeRevision;
    if (rootPath === '') return;
    for (const dirPath of expandedPaths) {
      if (childCache.has(dirPath)) {
        void loadDir(dirPath, true);
      }
    }
    // expandedPaths / childCache intentionally read fresh; we only want this to
    // fire on a revision change, not when those sets shift.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [fileTreeRevision]);

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

  // ----- Flat visible-row model -------------------------------------------
  // A DFS over the tree honoring expansion, exclusions, filter and auto-nesting.
  // This is the single source the windowed renderer slices, and the model the
  // keyboard handler walks by index.
  const excludeRow = useCallback(
    (e: WorkspaceDirectoryEntry) =>
      excludedPatterns.some((p) => e.name.toLowerCase().includes(p.toLowerCase())),
    [excludedPatterns],
  );

  const flatRows = useMemo<FlatRow[]>(() => {
    if (workspaceRoot == null || workspaceRoot.path === '' || entries == null) {
      return [];
    }
    const rows: FlatRow[] = [];

    // Inline-create at the workspace root sits before the root entries.
    if (pendingCreate != null && pendingCreate.parentPath === workspaceRoot.path) {
      rows.push({ kind: 'create', path: `__create__:${workspaceRoot.path}`, depth: 0, createKind: pendingCreate.kind });
    }

    const visibleRoot = entries.filter((e) => !excludeRow(e));
    const rootLevel = resolveLevel(visibleRoot, workspaceRoot.path, treeOrder, filterLower, manualNesting);

    // Recurse a directory's children (lazily loaded) once it's expanded.
    const pushDir = (dirPath: string, depth: number) => {
      // Inline-create as the first child of this directory.
      if (pendingCreate != null && pendingCreate.parentPath === dirPath) {
        rows.push({ kind: 'create', path: `__create__:${dirPath}`, depth, createKind: pendingCreate.kind });
      }
      const err = childErrors.get(dirPath);
      if (err != null) {
        rows.push({ kind: 'error', path: `__error__:${dirPath}`, depth, message: err });
      }
      const loaded = childCache.get(dirPath);
      if (loaded == null) {
        return; // not loaded yet — spinner shows on the dir row itself
      }
      const level = resolveLevel(loaded.filter((c) => !excludeRow(c)), dirPath, treeOrder, filterLower, manualNesting);
      for (const child of level.roots) {
        pushEntry(child, depth, level.childrenOf.get(child.path));
      }
    };

    // Emit one entry row, then recurse into it if it's an expanded container.
    const pushEntry = (
      entry: WorkspaceDirectoryEntry,
      depth: number,
      nestedChildren: WorkspaceDirectoryEntry[] | undefined,
    ) => {
      if (!entry.isDirectory) {
        // In filter mode, hide files whose name doesn't match (dirs stay so
        // matches inside remain reachable — same as the old per-node rule).
        const nameMatches = filterLower === '' || entry.name.toLowerCase().includes(filterLower);
        if (!nameMatches) return;
        const nested = nestedChildren ?? [];
        const expandable = nested.length > 0;
        const expanded = expandable && expandedPaths.has(entry.path);
        rows.push({
          kind: 'entry',
          path: entry.path,
          entry,
          depth,
          isNested: false,
          expandable,
          expanded,
          nestedChildren: expandable ? nested : undefined,
        });
        if (expanded) {
          // One level: nested files are plain leaves (never containers).
          for (const child of nested) {
            rows.push({
              kind: 'entry',
              path: child.path,
              entry: child,
              depth: depth + 1,
              isNested: true,
              expandable: false,
              expanded: false,
            });
          }
        }
        return;
      }
      // Directory row.
      const expanded = expandedPaths.has(entry.path);
      rows.push({
        kind: 'entry',
        path: entry.path,
        entry,
        depth,
        isNested: false,
        expandable: true,
        expanded,
      });
      if (expanded) {
        pushDir(entry.path, depth + 1);
      }
    };

    for (const entry of rootLevel.roots) {
      pushEntry(entry, 0, rootLevel.childrenOf.get(entry.path));
    }
    return rows;
  }, [
    workspaceRoot,
    entries,
    pendingCreate,
    excludeRow,
    treeOrder,
    filterLower,
    manualNesting,
    childCache,
    childErrors,
    expandedPaths,
  ]);

  // Auto-expand directories while a filter is active so matches inside become
  // visible. Cascades: as a dir's children load and enter the flat list, they
  // get expanded too on the next pass. Centralized version of the old per-node
  // effect.
  useEffect(() => {
    if (filterLower === '') return;
    const toExpand: string[] = [];
    for (const row of flatRows) {
      if (row.kind === 'entry' && row.entry.isDirectory && !row.expanded) {
        toExpand.push(row.entry.path);
      }
    }
    if (toExpand.length === 0) return;
    setExpandedPaths((prev) => {
      let changed = false;
      const next = new Set(prev);
      for (const p of toExpand) {
        if (!next.has(p)) {
          next.add(p);
          changed = true;
        }
      }
      return changed ? next : prev;
    });
  }, [filterLower, flatRows]);

  // Index of just the focusable rows (entry rows) for keyboard nav. Create /
  // error pseudo-rows are skipped so arrows move treeitem-to-treeitem.
  const entryRows = useMemo(
    () => flatRows.filter((r): r is Extract<FlatRow, { kind: 'entry' }> => r.kind === 'entry'),
    [flatRows],
  );

  // --- Reveal the active file in the tree (VS Code "reveal active file") -----
  // Fires ONLY when activeFilePath (or the workspace) changes — never on every
  // render — so it doesn't undo the user's manual collapses. Expands the whole
  // ancestor-folder chain at once: the lazy-load effect above iterates
  // expandedPaths directly, so every level loads in parallel even before its
  // parent is a visible row, and each level renders open as its children arrive.
  // The actual select + scroll happens in the resolver effect below, once the
  // target's row exists in the flat list.
  useEffect(() => {
    if (rootPath === '' || activeFilePath == null || activeFilePath === '') return;
    const ancestors = ancestorFoldersToReveal(activeFilePath, rootPath);
    if (ancestors.length > 0) {
      setExpandedPaths((prev) => {
        let changed = false;
        const next = new Set(prev);
        for (const p of ancestors) {
          if (!next.has(p)) {
            next.add(p);
            changed = true;
          }
        }
        return changed ? next : prev;
      });
    }
    setPendingReveal(activeFilePath);
    // Intentionally keyed only on the active file + root: re-running on
    // expandedPaths/flatRows would re-expand folders the user just collapsed.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [activeFilePath, rootPath]);

  // The active indent guide: trace the focused tree row (or, before any focus,
  // the open file) up to its parent and brighten that column for the parent's
  // whole subtree, so the path to the selected item reads stronger than the rest.
  const activeGuide = useMemo<ActiveGuide | null>(() => {
    const target = focusedPath ?? activeFilePath;
    if (target == null || target === '') return null;
    const row = entryRows.find((r) => r.path === target);
    if (row == null || row.depth <= 0) return null;
    const sepIdx = Math.max(target.lastIndexOf('/'), target.lastIndexOf('\\'));
    if (sepIdx < 0) return null;
    return { level: row.depth - 1, prefix: target.slice(0, sepIdx + 1) };
  }, [entryRows, focusedPath, activeFilePath]);

  // ----- Windowing --------------------------------------------------------
  const [scrollTop, setScrollTop] = useState(0);
  const [viewportH, setViewportH] = useState(0);
  const rowHRef = useRef(DEFAULT_ROW_H);
  const firstRowRef = useRef<HTMLDivElement>(null);

  // Measure the real row height once a row is on screen (font/zoom-proof). Only
  // matters in a real browser; jsdom reports 0, in which case we keep the
  // default and the "render everything" fallback (viewportH === 0) kicks in.
  useLayoutEffect(() => {
    const el = firstRowRef.current;
    if (el == null) return;
    const h = el.getBoundingClientRect().height;
    if (h > 0) rowHRef.current = h;
  });

  // Track the scroll container's height. ResizeObserver keeps windowing correct
  // when the panel is resized; the initial read seeds it after mount.
  useLayoutEffect(() => {
    const el = treeRootRef.current;
    if (el == null) return;
    const read = () => setViewportH(el.clientHeight);
    read();
    if (typeof ResizeObserver === 'undefined') return;
    const ro = new ResizeObserver(read);
    ro.observe(el);
    return () => ro.disconnect();
  }, [entries]);

  const onScroll = useCallback((e: React.UIEvent<HTMLDivElement>) => {
    setScrollTop(e.currentTarget.scrollTop);
  }, []);

  const rowH = rowHRef.current;
  const total = flatRows.length;
  // When we can't measure the viewport (jsdom, or the very first paint), render
  // everything. This is what keeps tests — which run in a zero-height jsdom —
  // seeing every row, and it's a safe fallback in production too.
  const windowed = viewportH > 0 && total > 0;
  const startIndex = windowed ? Math.max(0, Math.floor(scrollTop / rowH) - OVERSCAN) : 0;
  const endIndex = windowed
    ? Math.min(total, Math.ceil((scrollTop + viewportH) / rowH) + OVERSCAN)
    : total;
  const topPad = windowed ? startIndex * rowH : 0;
  const bottomPad = windowed ? Math.max(0, (total - endIndex) * rowH) : 0;
  const visibleRows = windowed ? flatRows.slice(startIndex, endIndex) : flatRows;

  // Resolve a pending "reveal active file": once the target's row appears in the
  // flat list (its ancestor folders have finished loading), select it (roving
  // tab stop + active highlight) and centre it in the viewport. Re-runs as rows
  // stream in. Does NOT pull DOM focus off the editor — VS Code likewise selects
  // the row but leaves keyboard focus where it is. Bails when the whole chain is
  // expanded + loaded yet the row never shows (file deleted/excluded), so a stale
  // target can't linger.
  useEffect(() => {
    if (pendingReveal == null) return;
    const idx = entryRows.findIndex((r) => r.path === pendingReveal);
    if (idx >= 0) {
      setFocusedPath(pendingReveal);
      // Centre only when the row is outside the current window — an already
      // visible file shouldn't jump.
      if (windowed) {
        const el = treeRootRef.current;
        const row = entryRows[idx];
        const flatIdx = row != null ? flatRows.indexOf(row) : -1;
        if (el != null && flatIdx >= 0) {
          const top = flatIdx * rowH;
          const bottom = top + rowH;
          if (top < el.scrollTop || bottom > el.scrollTop + viewportH) {
            const nextTop = Math.max(0, Math.round(top - viewportH / 2 + rowH / 2));
            el.scrollTop = nextTop;
            setScrollTop(nextTop);
          }
        }
      }
      setPendingReveal(null);
      return;
    }
    // Not found yet. If every ancestor is expanded and done loading (cached or
    // errored, none still in flight) the target genuinely isn't here — stop.
    const ancestors = ancestorFoldersToReveal(pendingReveal, rootPath);
    const settled = ancestors.every(
      (p) =>
        expandedPaths.has(p) &&
        !childLoading.has(p) &&
        (childCache.has(p) || childErrors.has(p)),
    );
    if (settled) setPendingReveal(null);
  }, [
    pendingReveal,
    entryRows,
    flatRows,
    expandedPaths,
    childCache,
    childErrors,
    childLoading,
    rootPath,
    windowed,
    rowH,
    viewportH,
  ]);

  // ----- Keyboard navigation (WAI-ARIA tree pattern) ----------------------
  // Index math over the flat entry list — not a DOM walk — so it works even when
  // the target row is scrolled out of the window and not currently rendered.
  // A pending-focus path is focused by a layout effect once its row mounts.
  const pendingFocusRef = useRef<string | null>(null);

  const requestFocus = useCallback((path: string) => {
    setFocusedPath(path);
    pendingFocusRef.current = path;
  }, []);

  // Once the windowed slice settles, focus the row we navigated to (it may have
  // just scrolled into view). In jsdom everything is rendered, so this runs in
  // the same commit and synchronous `toHaveFocus()` assertions pass.
  useLayoutEffect(() => {
    const path = pendingFocusRef.current;
    if (path == null) return;
    const root = treeRootRef.current;
    if (root == null) return;
    const el = root.querySelector<HTMLElement>(`[data-path="${cssEscape(path)}"]`);
    if (el != null) {
      el.focus();
      pendingFocusRef.current = null;
    }
  });

  const scrollIndexIntoView = useCallback(
    (index: number) => {
      if (!windowed) return;
      const el = treeRootRef.current;
      if (el == null) return;
      const top = index * rowH;
      const bottom = top + rowH;
      if (top < el.scrollTop) {
        el.scrollTop = top;
      } else if (bottom > el.scrollTop + viewportH) {
        el.scrollTop = bottom - viewportH;
      }
    },
    [windowed, rowH, viewportH],
  );

  const focusEntryAt = useCallback(
    (entryIdx: number) => {
      if (entryRows.length === 0) return;
      const clamped = Math.max(0, Math.min(entryRows.length - 1, entryIdx));
      const row = entryRows[clamped];
      if (row == null) return;
      // Translate the entry index into the full flat index for scroll math.
      const flatIdx = flatRows.indexOf(row);
      if (flatIdx >= 0) scrollIndexIntoView(flatIdx);
      requestFocus(row.path);
    },
    [entryRows, flatRows, scrollIndexIntoView, requestFocus],
  );

  const onTreeKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLDivElement>) => {
      if (entryRows.length === 0) return;
      const curPath = focusedPath ?? entryRows[0]?.path ?? null;
      const idx = curPath != null ? entryRows.findIndex((r) => r.path === curPath) : -1;
      switch (e.key) {
        case 'ArrowDown':
          e.preventDefault();
          focusEntryAt(idx + 1);
          break;
        case 'ArrowUp':
          e.preventDefault();
          focusEntryAt(idx - 1);
          break;
        case 'Home':
          e.preventDefault();
          focusEntryAt(0);
          break;
        case 'End':
          e.preventDefault();
          focusEntryAt(entryRows.length - 1);
          break;
        case 'ArrowRight': {
          if (idx < 0) break;
          e.preventDefault();
          const row = entryRows[idx]!;
          if (row.expandable && !row.expanded) {
            expandPath(row.path);
          } else if (row.expandable && row.expanded) {
            focusEntryAt(idx + 1);
          }
          break;
        }
        case 'ArrowLeft': {
          if (idx < 0) break;
          e.preventDefault();
          const row = entryRows[idx]!;
          if (row.expandable && row.expanded) {
            collapsePath(row.path);
          } else {
            // Jump to the nearest shallower ancestor row above.
            for (let i = idx - 1; i >= 0; i--) {
              if (entryRows[i]!.depth < row.depth) {
                focusEntryAt(i);
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
    [entryRows, focusedPath, focusEntryAt, expandPath, collapsePath],
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

  const firstEntryPath = entryRows[0]?.path ?? null;
  // First real (entry) row in the on-screen slice — gets the height-measuring ref
  // (skips create/error pseudo-rows so the measurement reflects a true row).
  const firstSliceEntryPath = visibleRows.find((r) => r.kind === 'entry')?.path ?? null;

  return (
    <GitDecorationsProvider workspaceRoot={workspaceRoot.path}>
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
          onScroll={onScroll}
          onContextMenu={(e) => openCtx(e, workspaceRoot.path, true)}
          onDragOver={onRootDragOver}
          onDragLeave={onRootDragLeave}
          onDrop={onRootDrop}
          style={{
            outline: rootDragOver ? '1px dashed rgba(255,255,255,0.28)' : undefined,
            background: rootDragOver ? 'rgba(255,255,255,0.05)' : undefined,
          }}
        >
          {/* Spacer for the rows scrolled off the top of the window. */}
          {topPad > 0 ? <div style={{ height: topPad }} aria-hidden="true" /> : null}
          <ul className={styles.treeList} role="group">
            {visibleRows.map((row) => {
              if (row.kind === 'create') {
                return (
                  <li key={row.path} className={styles.treeItem} role="none">
                    <InlineCreateRow
                      kind={row.createKind}
                      depth={row.depth}
                      onSubmit={(name) => void commitPendingCreate(name)}
                      onCancel={cancelPendingCreate}
                    />
                  </li>
                );
              }
              if (row.kind === 'error') {
                return (
                  <li key={row.path} className={styles.treeItem} role="none">
                    <p
                      className={styles.childError}
                      role="alert"
                      style={{ paddingLeft: `${rowPadRem(row.depth) + 1}rem` }}
                    >
                      {row.message}
                    </p>
                  </li>
                );
              }
              return (
                <li key={row.path} className={styles.treeItem} role="none">
                  <Row
                    row={row}
                    activeFilePath={activeFilePath}
                    activeGuide={activeGuide}
                    isFirstRow={row.path === firstEntryPath}
                    childLoading={childLoading.has(row.path)}
                    measureRef={row.path === firstSliceEntryPath ? firstRowRef : undefined}
                    onOpenFile={openFileFromWorkspace}
                    onContextMenu={openCtx}
                    onMoveInto={handleMoveInto}
                    onNestUnder={handleNestUnder}
                  />
                </li>
              );
            })}
          </ul>
          {/* Spacer for the rows below the window. */}
          {bottomPad > 0 ? <div style={{ height: bottomPad }} aria-hidden="true" /> : null}
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
    </GitDecorationsProvider>
  );
}

/** CSS.escape shim — guards the attribute selector used to focus a row by path
 *  (paths contain `\`, spaces, dots). Falls back to a manual escape in the
 *  unlikely event CSS.escape is missing (older jsdom). */
function cssEscape(value: string): string {
  if (typeof CSS !== 'undefined' && typeof CSS.escape === 'function') {
    return CSS.escape(value);
  }
  return value.replace(/["\\]/g, '\\$&');
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

const DRAG_MIME = 'application/x-cremniy-tree-path';
// Present on the drag payload only when the dragged item is a FILE — lets a file
// row light up as a nest target (dragover can read `types` but not the data).
const DRAG_FILE_MIME = 'application/x-cremniy-tree-file';

function decoColorClass(kind: GitDecoKind): string {
  switch (kind) {
    case 'untracked':
      return styles.decoUntracked;
    case 'added':
      return styles.decoAdded;
    case 'deleted':
      return styles.decoDeleted;
    case 'conflict':
      return styles.decoConflict;
    case 'renamed':
    case 'modified':
    default:
      return styles.decoModified;
  }
}

/**
 * The name text of a tree row, decorated by git status. A direct change (file)
 * keeps the tinted name + trailing status letter; a rolled-up folder shows the
 * plain name plus a small right-aligned coloured dot (trees.software-style).
 * Falls back to the plain name when there's no decoration.
 */
function RowLabel({
  deco,
  name,
  baseClass,
}: {
  deco: GitDecoResult | null;
  name: string;
  baseClass: string;
}) {
  if (deco == null) {
    return <span className={baseClass}>{name}</span>;
  }
  const colorClass = decoColorClass(deco.deco.kind);
  if (deco.rollup) {
    return (
      <>
        <span className={baseClass}>{name}</span>
        <span className={`${styles.decoDot} ${colorClass}`} aria-hidden="true" />
      </>
    );
  }
  const struck = deco.deco.kind === 'deleted';
  return (
    <>
      <span
        className={`${baseClass} ${colorClass}`}
        style={struck ? { textDecoration: 'line-through' } : undefined}
      >
        {name}
      </span>
      <span className={`${styles.decoBadge} ${colorClass}`} aria-hidden="true">
        {deco.deco.letter}
      </span>
    </>
  );
}

/** The indent-guide column that traces the focused/active item: `level` is the
 *  parent's depth column, brightened for every row whose path is under `prefix`
 *  (the parent's subtree) — VS Code's active indent guide. */
type ActiveGuide = { level: number; prefix: string };

/** Per-row indent guides: one faint vertical line per ancestor level, replacing
 *  the old per-<ul> guide (the flat list has no nested lists). Lines sit at the
 *  same columns the recursive guide used: parent_pad + 0.6rem. The column leading
 *  to the selected item (`activeGuide`) is drawn brighter so it reads as the
 *  active path through the open folders. */
function IndentGuides({
  depth,
  path,
  activeGuide,
}: {
  depth: number;
  path: string;
  activeGuide: ActiveGuide | null;
}) {
  if (depth <= 0) return null;
  const activeLevel =
    activeGuide != null && path.startsWith(activeGuide.prefix) ? activeGuide.level : -1;
  const lines = [];
  for (let d = 0; d < depth; d++) {
    lines.push(
      <span
        key={d}
        aria-hidden="true"
        className={d === activeLevel ? styles.indentGuideActive : styles.indentGuide}
        style={{ left: `${rowPadRem(d) + 0.6}rem` }}
      />,
    );
  }
  return <>{lines}</>;
}

type RowProps = {
  row: Extract<FlatRow, { kind: 'entry' }>;
  activeFilePath: string | null;
  /** Seeds the roving tab stop before the user has focused any row. */
  isFirstRow: boolean;
  /** This directory's first child listing is in flight (shows the dir spinner). */
  childLoading: boolean;
  /** Attached to the first rendered row so its real height can be measured. */
  measureRef?: React.Ref<HTMLDivElement>;
  onOpenFile: (path: string, opts?: { preview?: boolean }) => Promise<void>;
  onContextMenu: (ev: MouseEvent, path: string, isDirectory: boolean, isNested?: boolean) => void;
  onMoveInto: (sourcePath: string, targetDirectory: string) => Promise<void>;
  /** Drag a sibling file onto another file → nest it under that file. */
  onNestUnder: (sourcePath: string, targetPath: string) => void;
  /** The brightened indent-guide column tracing the focused/active item. */
  activeGuide: ActiveGuide | null;
};

/**
 * One flattened tree row — directory, plain file, or a file that owns nested
 * children. This is the recursive FileTreeNode's JSX, lifted out so the windowed
 * list can render it: identical visuals, classes, data-attributes, drag/drop and
 * click/keyboard behaviour. All recursion now lives in the flat-row builder.
 */
function Row({
  row,
  activeFilePath,
  activeGuide,
  isFirstRow,
  childLoading,
  measureRef,
  onOpenFile,
  onContextMenu,
  onMoveInto,
  onNestUnder,
}: RowProps) {
  const nav = useTreeNav();
  const { entry, depth, expanded, expandable, isNested } = row;
  const deco = useGitDecoration(entry.path, entry.isDirectory);
  const isTabStop = nav.focusedPath === entry.path || (nav.focusedPath == null && isFirstRow);
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

  // Directory drop target (move into folder).
  const onDirDragOver = useCallback(
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

  const onDirDrop = useCallback(
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
      // Stop the drop from bubbling to an ancestor / the root tree. Without this
      // the same rename fires twice; the second errors with "from_path not
      // found" because the file already moved.
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

  const paddingRem = rowPadRem(depth);

  // ----- Directory row -----
  if (entry.isDirectory) {
    return (
      <button
        type="button"
        ref={measureRef as React.Ref<HTMLButtonElement> | undefined}
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
        onDragOver={onDirDragOver}
        onDragLeave={onDragLeave}
        onDrop={onDirDrop}
        onFocus={() => nav.setFocusedPath(entry.path)}
        onClick={() => nav.toggleExpand(entry.path)}
        onContextMenu={(e) => onContextMenu(e, entry.path, true)}
        title={entry.name}
      >
        <IndentGuides depth={depth} path={entry.path} activeGuide={activeGuide} />
        <ChevronIcon open={expanded} />
        <FolderIcon open={expanded} />
        <RowLabel deco={deco} name={entry.name} baseClass={styles.dirName} />
        {childLoading ? <span className={styles.dirBusy}> …</span> : null}
      </button>
    );
  }

  const isActive = activeFilePath === entry.path;

  // ----- File that owns auto-nested children (twistie expands its siblings) -----
  if (expandable) {
    return (
      <div
        ref={measureRef}
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
        onClick={() => void onOpenFile(entry.path, { preview: true })}
        onDoubleClick={() => void onOpenFile(entry.path)}
        onKeyDown={(e) => {
          if (e.key === 'Enter') void onOpenFile(entry.path);
        }}
        onContextMenu={(e) => onContextMenu(e, entry.path, false, isNested)}
        title={entry.name}
      >
        <IndentGuides depth={depth} path={entry.path} activeGuide={activeGuide} />
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
        <RowLabel deco={deco} name={entry.name} baseClass={styles.leafName} />
      </div>
    );
  }

  // ----- Plain file leaf -----
  return (
    <button
      type="button"
      ref={measureRef as React.Ref<HTMLButtonElement> | undefined}
      role="treeitem"
      aria-selected={isActive}
      className={`${styles.leafButton} ${isActive ? styles.leafButtonActive : ''}`}
      style={{
        paddingLeft: `${paddingRem + LEAF_GLYPH_REM}rem`,
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
      onClick={() => void onOpenFile(entry.path, { preview: true })}
      onDoubleClick={() => void onOpenFile(entry.path)}
      onKeyDown={(e) => {
        if (e.key === 'Enter') {
          e.preventDefault();
          void onOpenFile(entry.path);
        }
      }}
      onContextMenu={(e) => onContextMenu(e, entry.path, false, isNested)}
      title={entry.name}
    >
      <IndentGuides depth={depth} path={entry.path} activeGuide={activeGuide} />
      <FileIcon name={entry.name} />
      <RowLabel deco={deco} name={entry.name} baseClass={styles.leafName} />
    </button>
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
  const paddingRem = rowPadRem(depth) + LEAF_GLYPH_REM;
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
