import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useRef,
  useState,
  type ReactNode,
} from 'react';
import { useNavigate } from 'react-router-dom';

import {
  DEFAULT_APP_PREFERENCES,
  withOpenedWorkspacePinned,
} from '@domain/preferences/appPreferences';
import {
  initialGroupsState,
  getActiveGroup,
  getGroup,
  isPathOpenAnywhere,
  openInGroup,
  activateInGroup,
  openPanelInGroup,
  activatePanelInGroup as activatePanelInGroupState,
  closeInGroup,
  closePanelInGroup as closePanelInGroupState,
  reorderInGroup,
  activateGroup,
  renamePathInGroups,
  duplicateToNewGroup,
  moveTabBetweenGroups,
  nextGroupId,
  type GroupsState,
  type SplitSide,
} from '@domain/editor/editorGroups';
import type { FileMenuActionId } from '@domain/menu/fileMenu';
import { normalizeFsPath, parentDirectoryPath, fileNameFromPath } from '@domain/workspace/paths';
import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import {
  createProjectFolder,
  getWorkspaceFileSize,
  pickFile,
  pickFolder,
  pickSaveFile,
  readUserFile,
  readWorkspaceUserFile,
  writeUserFile,
} from '@infrastructure/tauri/bridge';

import { registerAgentCommands, registerAgentState } from '@shared/agent/agentBridge';
import { useNotify } from '@boundary/notifications/NotificationContext';

import { useWorkspaceRoot } from './WorkspaceContext';

/**
 * A read-only projection of one {@link EditorGroup} for consumers that render
 * groups (the editor-pane grid). Mirrors the group's view state; the document
 * buffers stay global and are read via {@link IdeSessionContextValue.getBuffer}.
 */
export type EditorGroupView = {
  id: string;
  openTabs: string[];
  openPanels: string[];
  activeFilePath: string | null;
  activePanel: string | null;
  previewFilePath: string | null;
};

export type IdeSessionContextValue = {
  activeFilePath: string | null;
  openFilePaths: string[];
  /** Pinned tabs render first and are protected from drag-reorder into the unpinned zone. */
  pinnedFilePaths: ReadonlySet<string>;
  togglePinFilePath: (filePath: string) => void;
  /** Reorder a tab inside its zone (pinned↔pinned or unpinned↔unpinned). */
  reorderOpenFiles: (fromIndex: number, toIndex: number) => void;
  documentText: string;
  dirtyFilePaths: string[];
  activeDocumentDirty: boolean;
  /** The active tab holds a non-text (binary) file — the code editor shows a
   *  placeholder and the Hex / Disassembler / Binary Tools read it from disk. */
  activeFileIsBinary: boolean;
  setDocumentText: (value: string) => void;
  openFileFromWorkspace: (filePath: string, opts?: { preview?: boolean }) => Promise<void>;
  /** Open a file (if needed) and reveal a 1-based line — used by Search. */
  openFileAtLine: (filePath: string, line: number) => Promise<void>;
  /** Set by openFileAtLine; the editor reveals it once the file is active. */
  revealTarget: { path: string; line: number; nonce: number } | null;
  /** Re-read clean (non-dirty) open files from disk — call after on-disk edits
   *  like Search → Replace so editor buffers don't go stale. */
  reloadCleanOpenBuffers: () => Promise<void>;
  /** Non-file center tabs (settings, etc.) opened in the editor "tab space". */
  openPanels: string[];
  activePanel: string | null;
  /** The single file tab in preview mode (italic; replaced on next single-click). */
  previewFilePath: string | null;
  openPanel: (id: string) => void;
  activatePanel: (id: string) => void;
  closePanel: (id: string) => void;
  activateOpenFile: (filePath: string) => void;
  closeOpenFile: (filePath: string) => void;
  closeOtherOpenFiles: (keepFilePath: string) => void;
  closeAllOpenFiles: () => void;
  // ── editor groups (VS Code-style 1D grid) ───────────────────────────────
  /** Every editor group, left→right (render order). One group in the default. */
  editorGroups: ReadonlyArray<EditorGroupView>;
  /** The focused group's id — the active-group projection above mirrors it. */
  activeGroupId: string;
  /** Focus a group (delegates to the pure `activateGroup`). */
  focusEditorGroup: (groupId: string) => void;
  /** Activate an open file tab inside a specific group. */
  activateFileInGroup: (groupId: string, filePath: string) => void;
  /** Close a file tab inside a specific group (same unsaved-changes confirm). */
  closeFileInGroup: (groupId: string, filePath: string) => void;
  /** Reorder a file tab within a specific group (caller keeps zones valid). */
  reorderFilesInGroup: (groupId: string, fromIndex: number, toIndex: number) => void;
  /** Re-focus an already-open center panel inside a specific group. */
  activatePanelInGroup: (groupId: string, id: string) => void;
  /** Close a center panel inside a specific group. */
  closePanelInGroup: (groupId: string, id: string) => void;
  /**
   * VS Code "Split Right/Left": open the active group's active file in a NEW
   * group on `side` (default `'right'`). The file stays in the source group too,
   * so both views share its global buffer. No-op when no file is active.
   */
  splitActiveFile: (side?: SplitSide) => void;
  /**
   * Move an open file tab from one group to another (Step 4 drag uses this).
   * `toIndex` inserts at a position in the target (default: end).
   */
  moveFileToGroup: (
    fromGroupId: string,
    toGroupId: string,
    filePath: string,
    toIndex?: number,
  ) => void;
  /**
   * Split a specific file off `sourceGroupId` into a NEW group on `side`,
   * sharing its buffer (Step 4 drag-to-edge uses this). The file stays in the
   * source group.
   */
  splitFileToNewGroup: (sourceGroupId: string, filePath: string, side: SplitSide) => void;
  /** Read a path's global document buffer (`''` when none). */
  getBuffer: (filePath: string) => string;
  /** Write a path's global document buffer (promotes preview, mirrors active). */
  writeBuffer: (filePath: string, text: string) => void;
  /** True when the path is a binary tab (no text buffer; shown via byte tools). */
  isBinaryPath: (filePath: string) => boolean;
  runFileMenuAction: (id: FileMenuActionId) => Promise<void>;
  fileTreeRevision: number;
  bumpFileTreeRevision: () => void;
  /**
   * Increments every time on-disk file content changes (after Save / write).
   * Tabs that load file bytes (Symbols, MemoryMap, Functions, Strings, Disasm)
   * watch this in their useEffect deps so they re-read instead of showing
   * stale data. Qt parity: refreshDataAllTabsSignal.
   */
  fileContentRevision: number;
  bumpFileContentRevision: () => void;
};

const IdeSessionContext = createContext<IdeSessionContextValue | null>(null);

/** The single editor group all view state lives in (step-1: no splits yet). */
const GROUP_ID = 'g0';

function formatUserMessage(error: unknown): string {
  if (error instanceof Error) {
    return error.message;
  }
  return String(error);
}

export function IdeSessionProvider({ children }: { children: ReactNode }) {
  const navigate = useNavigate();
  const workspaceRoot = useWorkspaceRoot();
  const notify = useNotify();
  // The editor *view* state — open tabs, active file, preview tab, center
  // panels, and the focus-order MRU — lives in one pure GroupsState with a
  // single group. The public fields below are derived from its active group;
  // mutators drive it through the pure functions in `domain/editor/editorGroups`.
  const [groups, setGroups] = useState<GroupsState>(() => initialGroupsState(GROUP_ID));
  const [pinnedFilePaths, setPinnedFilePaths] = useState<Set<string>>(() => new Set());
  const [buffers, setBuffers] = useState<Record<string, string>>({});
  const [savedBuffers, setSavedBuffers] = useState<Record<string, string>>({});
  // Tabs whose file is binary (non-UTF-8, or text that carries NUL bytes). They
  // have no text buffer — the byte tools read them straight from disk.
  const [binaryTabs, setBinaryTabs] = useState<Set<string>>(() => new Set());
  // documentText mirrors the active file's buffer; kept as its own state (not
  // derived) and refreshed whenever the active file changes — minimal-risk and
  // matches the legacy single source of truth the editor binds to.
  const [documentText, setDocumentTextState] = useState('');
  const [fileTreeRevision, setFileTreeRevision] = useState(0);
  const [fileContentRevision, setFileContentRevision] = useState(0);

  // Synchronous mirror of `groups` so callbacks read the current view state
  // without stale closures (the pure functions take the current state in and
  // return the next, which we both commit and store here).
  const groupsRef = useRef<GroupsState>(groups);
  const buffersRef = useRef<Record<string, string>>({});
  const savedBuffersRef = useRef<Record<string, string>>({});
  // UTF-8 BOM preservation: each open path remembers whether its on-disk
  // bytes started with EF BB BF, so save() can re-emit the BOM.
  const bomFlagsRef = useRef<Record<string, boolean>>({});

  useEffect(() => {
    groupsRef.current = groups;
  }, [groups]);

  useEffect(() => {
    buffersRef.current = buffers;
  }, [buffers]);

  useEffect(() => {
    savedBuffersRef.current = savedBuffers;
  }, [savedBuffers]);

  // Commit a new GroupsState to both the ref (for synchronous reads in the same
  // callback) and React state. Returns the committed state for chaining.
  const commitGroups = useCallback((next: GroupsState): GroupsState => {
    groupsRef.current = next;
    setGroups(next);
    return next;
  }, []);

  // ── derived view state (always reflects the active group) ────────────────
  const activeGroup = getActiveGroup(groups);
  const activeFilePath = activeGroup.activeFilePath;
  const openTabs = activeGroup.openTabs;
  const previewFilePath = activeGroup.previewFilePath;
  const openPanels = activeGroup.openPanels;
  const activePanel = activeGroup.activePanel;
  const activeGroupId = groups.activeGroupId;

  // A read-only projection of every group for the editor-pane grid. Each entry
  // is a new object only when its group's view state changes (groups identity is
  // stable across unrelated state updates), so consumers can memo on g.id.
  const editorGroups = useMemo<ReadonlyArray<EditorGroupView>>(
    () =>
      groups.groups.map((g) => ({
        id: g.id,
        openTabs: g.openTabs,
        openPanels: g.openPanels,
        activeFilePath: g.activeFilePath,
        activePanel: g.activePanel,
        previewFilePath: g.previewFilePath,
      })),
    [groups],
  );

  // Reflect the active file's buffer into documentText whenever the model says
  // a (different) file is active — drives the editor binding the way the legacy
  // setActiveFilePath + setDocumentTextState pair did, but for any state change
  // (close re-pick, save-as rename, split, …) routed through the model.
  const showActiveBuffer = useCallback((state: GroupsState): void => {
    const g = getActiveGroup(state);
    if (g.activePanel != null) {
      return;
    }
    const path = g.activeFilePath;
    if (path == null || path === '') {
      setDocumentTextState('');
      return;
    }
    if (binaryTabs.has(path)) {
      setDocumentTextState('');
      return;
    }
    setDocumentTextState(buffersRef.current[path] ?? '');
  }, [binaryTabs]);

  const bumpFileTreeRevision = useCallback(() => {
    setFileTreeRevision((n) => n + 1);
  }, []);
  const bumpFileContentRevision = useCallback(() => {
    setFileContentRevision((n) => n + 1);
  }, []);

  // Keep the Explorer + git in sync with the disk without a manual Refresh:
  // bump on window focus (caught up after editing elsewhere) and on a gentle
  // poll while the window is focused (catches a build / terminal writing files).
  useEffect(() => {
    const onFocus = () => bumpFileTreeRevision();
    window.addEventListener('focus', onFocus);
    const poll = window.setInterval(() => {
      if (document.hasFocus()) {
        bumpFileTreeRevision();
      }
    }, 4000);
    return () => {
      window.removeEventListener('focus', onFocus);
      window.clearInterval(poll);
    };
  }, [bumpFileTreeRevision]);

  const togglePinFilePath = useCallback((filePath: string) => {
    setPinnedFilePaths((prev) => {
      const next = new Set(prev);
      if (next.has(filePath)) next.delete(filePath);
      else next.add(filePath);
      return next;
    });
  }, []);

  // Swap two tabs in a group. Caller must ensure both indices live in the same
  // zone (both pinned or both unpinned) — the strip enforces that.
  const reorderFilesInGroup = useCallback(
    (groupId: string, fromIndex: number, toIndex: number) => {
      commitGroups(reorderInGroup(groupsRef.current, groupId, fromIndex, toIndex));
    },
    [commitGroups],
  );

  const reorderOpenFiles = useCallback(
    (fromIndex: number, toIndex: number) => {
      reorderFilesInGroup(groupsRef.current.activeGroupId, fromIndex, toIndex);
    },
    [reorderFilesInGroup],
  );

  const navigateToWorkspace = useCallback(
    (rootPath: string) => {
      navigate(`/ide?root=${encodeURIComponent(rootPath)}`);
    },
    [navigate],
  );

  const persistRecentAndNavigate = useCallback(
    async (rootPath: string) => {
      const prefsLoaded = await loadPreferences().catch(() => DEFAULT_APP_PREFERENCES);
      const next = withOpenedWorkspacePinned(prefsLoaded, rootPath);
      await savePreferences(next);
      navigateToWorkspace(rootPath);
    },
    [navigateToWorkspace],
  );

  const activateFileInGroup = useCallback(
    (groupId: string, filePath: string) => {
      const path = filePath.trim();
      if (path === '') {
        return;
      }
      const next = activateInGroup(groupsRef.current, groupId, path);
      if (next === groupsRef.current) {
        return; // not open here — no-op (mirrors the old openTabs guard)
      }
      commitGroups(next);
      // showActiveBuffer is a no-op unless this changed the *active* group's file.
      showActiveBuffer(next);
    },
    [commitGroups, showActiveBuffer],
  );

  const activateOpenFile = useCallback(
    (filePath: string) => {
      activateFileInGroup(groupsRef.current.activeGroupId, filePath);
    },
    [activateFileInGroup],
  );

  const closeFileInGroup = useCallback(
    (groupId: string, filePath: string) => {
      const path = filePath.trim();
      if (path === '') {
        return;
      }
      const g = getGroup(groupsRef.current, groupId);
      if (g == null || !g.openTabs.includes(path)) {
        return; // not open in this group — nothing to close.
      }

      const currentBuffer = buffersRef.current[path];
      const saved = savedBuffersRef.current[path];
      if ((currentBuffer ?? '') !== (saved ?? '')) {
        const name = fileNameFromPath(path) || path;
        if (!window.confirm(`Discard unsaved changes in "${name}"?`)) {
          return;
        }
      }

      const next = closeInGroup(groupsRef.current, groupId, path);
      commitGroups(next);

      // Buffer GC: only drop the global buffer/saved/binary/bom entries when the
      // path is no longer open in ANY group (always true with one group, but
      // coded correctly so splits in a later step stay safe).
      if (!isPathOpenAnywhere(next, path)) {
        setBuffers((prev) => {
          if (!(path in prev)) return prev;
          const n = { ...prev };
          delete n[path];
          return n;
        });
        setSavedBuffers((prev) => {
          if (!(path in prev)) return prev;
          const n = { ...prev };
          delete n[path];
          return n;
        });
        setBinaryTabs((prev) => {
          if (!prev.has(path)) return prev;
          const n = new Set(prev);
          n.delete(path);
          return n;
        });
        if (path in bomFlagsRef.current) {
          const n = { ...bomFlagsRef.current };
          delete n[path];
          bomFlagsRef.current = n;
        }
        // Pinning is global; only forget it once the tab is gone everywhere
        // (with one group this is the only group, so it's always reached).
        setPinnedFilePaths((prev) => {
          if (!prev.has(path)) return prev;
          const n = new Set(prev);
          n.delete(path);
          return n;
        });
      }

      // No-op unless this changed the *active* group's visible file.
      showActiveBuffer(next);
    },
    [commitGroups, showActiveBuffer],
  );

  const closeOpenFile = useCallback(
    (filePath: string) => {
      closeFileInGroup(groupsRef.current.activeGroupId, filePath);
    },
    [closeFileInGroup],
  );

  const closeOtherOpenFiles = useCallback(
    (keepFilePath: string) => {
      const keep = keepFilePath.trim();
      const tabs = getActiveGroup(groupsRef.current).openTabs.slice();
      for (const t of tabs) {
        if (t !== keep && !pinnedFilePaths.has(t)) {
          closeOpenFile(t);
        }
      }
    },
    [closeOpenFile, pinnedFilePaths],
  );

  const closeAllOpenFiles = useCallback(() => {
    const tabs = getActiveGroup(groupsRef.current).openTabs.slice();
    for (const t of tabs) {
      if (!pinnedFilePaths.has(t)) {
        closeOpenFile(t);
      }
    }
  }, [closeOpenFile, pinnedFilePaths]);

  const getBuffer = useCallback((filePath: string): string => buffersRef.current[filePath] ?? '', []);

  const isBinaryPath = useCallback((filePath: string): boolean => binaryTabs.has(filePath), [binaryTabs]);

  const focusEditorGroup = useCallback(
    (groupId: string) => {
      const next = activateGroup(groupsRef.current, groupId);
      if (next === groupsRef.current) {
        return;
      }
      commitGroups(next);
      // The active group changed → mirror its active file into documentText.
      showActiveBuffer(next);
    },
    [commitGroups, showActiveBuffer],
  );

  // Split a specific file off a group into a fresh group, keeping it in the
  // source (shared buffer). The new group becomes active.
  const splitFileToNewGroup = useCallback(
    (sourceGroupId: string, filePath: string, side: SplitSide) => {
      const path = filePath?.trim() ?? '';
      if (path === '') {
        return;
      }
      const next = duplicateToNewGroup(groupsRef.current, sourceGroupId, path, side, nextGroupId());
      if (next === groupsRef.current) {
        return; // path not open in that group, or id collision — no-op
      }
      commitGroups(next);
      // The new (active) group shows the same file — its buffer is already the
      // active one, so documentText needn't change, but keep the mirror honest.
      showActiveBuffer(next);
    },
    [commitGroups, showActiveBuffer],
  );

  // VS Code Split Right/Left on the active group's active file.
  const splitActiveFile = useCallback(
    (side: SplitSide = 'right') => {
      const g = getActiveGroup(groupsRef.current);
      const path = g.activeFilePath?.trim() ?? '';
      if (path === '') {
        return; // nothing active to split (scratch buffer or empty group)
      }
      splitFileToNewGroup(g.id, path, side);
    },
    [splitFileToNewGroup],
  );

  // Move a tab between groups (Step 4 drag). Re-mirrors documentText since the
  // target becomes active.
  const moveFileToGroup = useCallback(
    (fromGroupId: string, toGroupId: string, filePath: string, toIndex?: number) => {
      const path = filePath?.trim() ?? '';
      if (path === '') {
        return;
      }
      const next = moveTabBetweenGroups(groupsRef.current, fromGroupId, toGroupId, path, toIndex);
      if (next === groupsRef.current) {
        return;
      }
      commitGroups(next);
      showActiveBuffer(next);
    },
    [commitGroups, showActiveBuffer],
  );

  // Write a path's global buffer (used by any group's editor). Promotes a
  // preview tab to permanent in every group previewing this path once the text
  // diverges from disk, and mirrors documentText when the path is the active
  // group's visible file (the legacy single-source-of-truth the editor binds to).
  const writeBuffer = useCallback(
    (filePath: string, text: string) => {
      const path = filePath?.trim() ?? '';
      if (path === '') {
        return;
      }
      setBuffers((prev) => ({ ...prev, [path]: text }));

      if (getActiveGroup(groupsRef.current).activeFilePath === path) {
        setDocumentTextState(text);
      }

      // Editing a previewed file promotes it to a permanent tab — clear the
      // preview flag in every group that previews this path once the text
      // diverges from saved. openInGroup(preview=false) on an open path promotes
      // it in place.
      if (text !== (savedBuffersRef.current[path] ?? '')) {
        let state = groupsRef.current;
        let changed = false;
        for (const g of state.groups) {
          if (g.previewFilePath === path) {
            const after = openInGroup(state, g.id, path);
            if (after !== state) {
              state = after;
              changed = true;
            }
          }
        }
        if (changed) {
          // openInGroup focuses the touched group; with one group that's a no-op.
          // Re-home the active group so writing doesn't steal focus across groups.
          state = activateGroup(state, groupsRef.current.activeGroupId);
          commitGroups(state);
        }
      }
    },
    [commitGroups],
  );

  const setDocumentText = useCallback(
    (value: string) => {
      // Generalized: write the active group's active-file buffer. When no file
      // is active (scratch buffer), still reflect the typed text into the mirror.
      const path = getActiveGroup(groupsRef.current).activeFilePath ?? '';
      if (path === '') {
        setDocumentTextState(value);
        return;
      }
      writeBuffer(path, value);
    },
    [writeBuffer],
  );

  const openWorkspaceFolderFlow = useCallback(async () => {
    const path = await pickFolder();
    if (path == null || path === '') {
      return;
    }
    commitGroups(initialGroupsState(GROUP_ID));
    setBuffers({});
    setSavedBuffers({});
    setDocumentTextState('');
    await persistRecentAndNavigate(path);
  }, [commitGroups, persistRecentAndNavigate]);

  const newProjectFlow = useCallback(async () => {
    // Two-step prompt — pick parent folder, then ask for the new project name.
    // Backend (`createProjectFolder`) validates the name and creates the dir.
    const parent = await pickFolder();
    if (parent == null || parent === '') {
      return;
    }
    const name = window.prompt('New project folder name');
    if (name == null || name.trim() === '') {
      return;
    }
    try {
      const created = await createProjectFolder(parent, name.trim());
      commitGroups(initialGroupsState(GROUP_ID));
      setBuffers({});
      setSavedBuffers({});
      setDocumentTextState('');
      await persistRecentAndNavigate(created);
    } catch (e) {
      notify.error('Could not create project', formatUserMessage(e));
    }
  }, [commitGroups, notify, persistRecentAndNavigate]);

  const openFileFlow = useCallback(async () => {
    const path = await pickFile();
    if (path == null || path === '') {
      return;
    }
    const parent = parentDirectoryPath(path);
    if (parent === '') {
      notify.error('Could not determine folder for the selected file.');
      return;
    }
    const text = await readUserFile(path);
    const currentRoot = workspaceRoot?.path ?? '';
    if (normalizeFsPath(parent) !== normalizeFsPath(currentRoot)) {
      commitGroups(initialGroupsState(GROUP_ID));
      setBuffers({});
      setSavedBuffers({});
      setDocumentTextState('');
    }
    await persistRecentAndNavigate(parent);
    if (getActiveGroup(groupsRef.current).openTabs.includes(path)) {
      activateOpenFile(path);
      return;
    }
    const next = openInGroup(groupsRef.current, GROUP_ID, path);
    commitGroups(next);
    setBuffers((b) => ({ ...b, [path]: text }));
    setSavedBuffers((b) => ({ ...b, [path]: text }));
    setDocumentTextState(text);
  }, [activateOpenFile, commitGroups, notify, persistRecentAndNavigate, workspaceRoot?.path]);

  const saveDocumentAsFlow = useCallback(async () => {
    const chosen = await pickSaveFile(activeFilePath ?? null);
    if (chosen == null || chosen === '') {
      return;
    }
    const oldPath = activeFilePath;
    const hasBom =
      (oldPath != null && bomFlagsRef.current[oldPath]) ||
      bomFlagsRef.current[chosen] ||
      false;
    const payload = hasBom ? `﻿${documentText}` : documentText;
    await writeUserFile(chosen, payload);
    if (hasBom) {
      bomFlagsRef.current = { ...bomFlagsRef.current, [chosen]: true };
    }
    if (oldPath != null && oldPath !== '') {
      // Rename the tab in every group (merges into an existing `chosen` tab if
      // one is open — renamePathInGroups drops the old tab in that case).
      const next = renamePathInGroups(groupsRef.current, oldPath, chosen);
      commitGroups(next);
      setBuffers((prev) => {
        const n = { ...prev };
        delete n[oldPath];
        n[chosen] = documentText;
        return n;
      });
      setSavedBuffers((prev) => {
        const n = { ...prev };
        if (chosen !== oldPath) {
          delete n[oldPath];
        }
        n[chosen] = documentText;
        return n;
      });
    } else {
      const next = openInGroup(groupsRef.current, GROUP_ID, chosen);
      commitGroups(next);
      setBuffers((prev) => ({ ...prev, [chosen]: documentText }));
      setSavedBuffers((prev) => ({ ...prev, [chosen]: documentText }));
    }
    setDocumentTextState(documentText);
  }, [activeFilePath, commitGroups, documentText]);

  const saveDocumentFlow = useCallback(async () => {
    if (activeFilePath == null || activeFilePath === '') {
      await saveDocumentAsFlow();
      return;
    }
    const hasBom = bomFlagsRef.current[activeFilePath] ?? false;
    const payload = hasBom ? `﻿${documentText}` : documentText;
    await writeUserFile(activeFilePath, payload);
    setSavedBuffers((prev) => ({ ...prev, [activeFilePath]: documentText }));
  }, [activeFilePath, documentText, saveDocumentAsFlow]);

  const [revealTarget, setRevealTarget] = useState<
    { path: string; line: number; nonce: number } | null
  >(null);
  const revealNonceRef = useRef(0);

  // Open a file as a binary tab: it gets a tab and becomes active (so the byte
  // tools pick it up via `activeFilePath`), but carries no text buffer — the
  // editor shows a placeholder instead of trying to render bytes as text.
  const openAsBinaryTab = useCallback(
    (filePath: string) => {
      const path = filePath.trim();
      if (path === '') {
        return;
      }
      const next = openInGroup(groupsRef.current, GROUP_ID, path);
      commitGroups(next);
      setBinaryTabs((prev) => {
        if (prev.has(path)) return prev;
        const n = new Set(prev);
        n.add(path);
        return n;
      });
      setDocumentTextState('');
    },
    [commitGroups],
  );

  const openFileFromWorkspace = useCallback(
    async (filePath: string, opts?: { preview?: boolean }) => {
      const trimmed = filePath.trim();
      if (trimmed === '') {
        return;
      }
      const rootPath = workspaceRoot?.path?.trim() ?? '';
      if (rootPath === '') {
        notify.warn('No workspace is open. Open a folder from the File menu first.');
        return;
      }
      // Single-click opens a preview tab; double-click / other callers open it
      // for keeps (default). Opening the current preview "for keeps" promotes it.
      const preview = opts?.preview ?? false;
      if (getActiveGroup(groupsRef.current).openTabs.includes(trimmed)) {
        // Re-open: activate, and (for-keeps) promote it if it was the preview.
        // openInGroup with preview=false handles both activation and promotion.
        const next = preview
          ? activateInGroup(groupsRef.current, GROUP_ID, trimmed)
          : openInGroup(groupsRef.current, GROUP_ID, trimmed);
        commitGroups(next);
        showActiveBuffer(next);
        return;
      }
      try {
        const raw = await readWorkspaceUserFile(rootPath, trimmed);
        // A file that decodes but still carries NUL bytes (firmware dumps,
        // UTF-16, …) is binary — route it to the byte tools, not the editor.
        if (raw.includes(String.fromCharCode(0))) {
          openAsBinaryTab(trimmed);
          return;
        }
        // Detect UTF-8 BOM (0xFEFF in JS string) so we can re-emit it on save.
        const hasBom = raw.charCodeAt(0) === 0xfeff;
        const text = hasBom ? raw.slice(1) : raw;
        bomFlagsRef.current = { ...bomFlagsRef.current, [trimmed]: hasBom };

        // A single-click preview reuses the current preview tab in place (when
        // it's clean), instead of piling up tabs. Dirty preview is kept — the
        // model only replaces a clean preview when we pass cleanPreview.
        const old = getActiveGroup(groupsRef.current).previewFilePath;
        const previewClean =
          old != null && (buffersRef.current[old] ?? '') === (savedBuffersRef.current[old] ?? '');
        const replacedOld =
          preview && old != null && old !== trimmed && previewClean;

        const next = openInGroup(groupsRef.current, GROUP_ID, trimmed, {
          preview,
          cleanPreview: previewClean,
        });
        commitGroups(next);

        // When a clean preview tab was replaced in place, GC its buffers (the
        // tab is gone). Safe-guarded by isPathOpenAnywhere for future splits.
        if (replacedOld && old != null && !isPathOpenAnywhere(next, old)) {
          setBuffers((b) => {
            const n = { ...b, [trimmed]: text };
            delete n[old];
            return n;
          });
          setSavedBuffers((b) => {
            const n = { ...b, [trimmed]: text };
            delete n[old];
            return n;
          });
        } else {
          setBuffers((b) => ({ ...b, [trimmed]: text }));
          setSavedBuffers((b) => ({ ...b, [trimmed]: text }));
        }

        // Opened as text — make sure it isn't still flagged binary (a file can
        // flip across reopens).
        setBinaryTabs((prev) => {
          if (!prev.has(trimmed)) return prev;
          const n = new Set(prev);
          n.delete(trimmed);
          return n;
        });
        setDocumentTextState(text);
      } catch (e) {
        // The text decode failed — most often invalid UTF-8, i.e. an
        // executable. If the file is readable at all, open it as a binary tab
        // so the Hex / Disassembler / Binary Tools can analyse it; only a
        // genuinely unreadable file (missing, no permission) is an error.
        try {
          await getWorkspaceFileSize(rootPath, trimmed);
          openAsBinaryTab(trimmed);
        } catch {
          notify.error('Could not open file', formatUserMessage(e));
        }
      }
    },
    [notify, workspaceRoot?.path, commitGroups, showActiveBuffer, openAsBinaryTab],
  );

  const openFileAtLine = useCallback(
    async (filePath: string, line: number) => {
      await openFileFromWorkspace(filePath);
      revealNonceRef.current += 1;
      setRevealTarget({ path: filePath.trim(), line, nonce: revealNonceRef.current });
    },
    [openFileFromWorkspace],
  );

  const reloadCleanOpenBuffers = useCallback(async () => {
    const root = workspaceRoot?.path?.trim() ?? '';
    if (root === '') {
      return;
    }
    // Only files with no unsaved edits — never overwrite the user's buffer.
    const clean = getActiveGroup(groupsRef.current).openTabs.filter(
      (p) => (buffersRef.current[p] ?? '') === (savedBuffersRef.current[p] ?? ''),
    );
    for (const p of clean) {
      try {
        const raw = await readWorkspaceUserFile(root, p);
        const hasBom = raw.charCodeAt(0) === 0xfeff;
        const text = hasBom ? raw.slice(1) : raw;
        bomFlagsRef.current = { ...bomFlagsRef.current, [p]: hasBom };
        setBuffers((b) => ({ ...b, [p]: text }));
        setSavedBuffers((b) => ({ ...b, [p]: text }));
        if (getActiveGroup(groupsRef.current).activeFilePath === p) {
          setDocumentTextState(text);
        }
      } catch {
        // File may have been deleted/renamed — skip it.
      }
    }
    bumpFileContentRevision();
  }, [workspaceRoot?.path, bumpFileContentRevision]);

  const openPanel = useCallback(
    (id: string) => {
      commitGroups(openPanelInGroup(groupsRef.current, GROUP_ID, id));
    },
    [commitGroups],
  );

  const activatePanelInGroup = useCallback(
    (groupId: string, id: string) => {
      const next = activatePanelInGroupState(groupsRef.current, groupId, id);
      commitGroups(next);
      // No-op unless this changed the *active* group's overlay.
      showActiveBuffer(next);
    },
    [commitGroups, showActiveBuffer],
  );

  const activatePanel = useCallback(
    (id: string) => {
      activatePanelInGroup(groupsRef.current.activeGroupId, id);
    },
    [activatePanelInGroup],
  );

  const closePanelInGroup = useCallback(
    (groupId: string, id: string) => {
      const next = closePanelInGroupState(groupsRef.current, groupId, id);
      commitGroups(next);
      // Closing the visible panel re-picks a file/panel — refresh documentText.
      showActiveBuffer(next);
    },
    [commitGroups, showActiveBuffer],
  );

  const closePanel = useCallback(
    (id: string) => {
      closePanelInGroup(groupsRef.current.activeGroupId, id);
    },
    [closePanelInGroup],
  );

  const closeWorkspaceFlow = useCallback(() => {
    commitGroups(initialGroupsState(GROUP_ID));
    setBuffers({});
    setSavedBuffers({});
    setDocumentTextState('');
    navigate('/', { replace: true });
  }, [commitGroups, navigate]);

  const closeEditorTabFlow = useCallback(() => {
    const g = getActiveGroup(groupsRef.current);
    const tabs = g.openTabs;
    const path = g.activeFilePath?.trim() ?? '';

    const closeScratch = (): void => {
      const text = documentText ?? '';
      if (text.trim() === '') {
        return;
      }
      if (!window.confirm('Discard the current editor contents?')) {
        return;
      }
      setDocumentTextState('');
    };

    if (path !== '') {
      closeOpenFile(path);
      return;
    }
    if (tabs.length > 0) {
      closeOpenFile(tabs[0]!);
      return;
    }

    closeScratch();
  }, [closeOpenFile, documentText]);

  const runFileMenuAction = useCallback(
    async (id: FileMenuActionId) => {
      try {
        switch (id) {
          case 'newProject':
            await newProjectFlow();
            return;
          case 'openFolder':
            await openWorkspaceFolderFlow();
            return;
          case 'openFile':
            await openFileFlow();
            return;
          case 'save':
            await saveDocumentFlow();
            return;
          case 'saveAs':
            await saveDocumentAsFlow();
            return;
          case 'preferences':
            return;
          case 'closeEditorTab':
            closeEditorTabFlow();
            return;
          case 'closeWorkspace':
            closeWorkspaceFlow();
            return;
          default: {
            const unreachable: never = id;
            return unreachable;
          }
        }
      } catch (e) {
        notify.error('File menu action failed', formatUserMessage(e));
      }
    },
    [
      closeEditorTabFlow,
      closeWorkspaceFlow,
      newProjectFlow,
      notify,
      openFileFlow,
      openWorkspaceFolderFlow,
      saveDocumentAsFlow,
      saveDocumentFlow,
    ],
  );

  const dirtyFilePaths = useMemo(
    () => openTabs.filter((path) => (buffers[path] ?? '') !== (savedBuffers[path] ?? '')),
    [buffers, openTabs, savedBuffers],
  );

  const activeDocumentDirty =
    activeFilePath != null && activeFilePath !== ''
      ? (buffers[activeFilePath] ?? '') !== (savedBuffers[activeFilePath] ?? '')
      : documentText !== '';

  const activeFileIsBinary =
    activeFilePath != null && activeFilePath !== '' && binaryTabs.has(activeFilePath);

  const value = useMemo<IdeSessionContextValue>(
    () => ({
      activeFilePath,
      openFilePaths: openTabs,
      pinnedFilePaths,
      togglePinFilePath,
      reorderOpenFiles,
      documentText,
      dirtyFilePaths,
      activeDocumentDirty,
      activeFileIsBinary,
      setDocumentText,
      openFileFromWorkspace,
      openFileAtLine,
      revealTarget,
      activateOpenFile,
      closeOpenFile,
      closeOtherOpenFiles,
      closeAllOpenFiles,
      runFileMenuAction,
      fileTreeRevision,
      bumpFileTreeRevision,
      fileContentRevision,
      bumpFileContentRevision,
      reloadCleanOpenBuffers,
      openPanels,
      activePanel,
      previewFilePath,
      openPanel,
      activatePanel,
      closePanel,
      editorGroups,
      activeGroupId,
      focusEditorGroup,
      activateFileInGroup,
      closeFileInGroup,
      reorderFilesInGroup,
      activatePanelInGroup,
      closePanelInGroup,
      splitActiveFile,
      moveFileToGroup,
      splitFileToNewGroup,
      getBuffer,
      writeBuffer,
      isBinaryPath,
    }),
    [
      activeDocumentDirty,
      activeFileIsBinary,
      activeFilePath,
      pinnedFilePaths,
      togglePinFilePath,
      reorderOpenFiles,
      dirtyFilePaths,
      openTabs,
      documentText,
      setDocumentText,
      openFileFromWorkspace,
      openFileAtLine,
      revealTarget,
      activateOpenFile,
      closeOpenFile,
      closeOtherOpenFiles,
      closeAllOpenFiles,
      runFileMenuAction,
      fileTreeRevision,
      bumpFileTreeRevision,
      fileContentRevision,
      bumpFileContentRevision,
      reloadCleanOpenBuffers,
      openPanels,
      activePanel,
      previewFilePath,
      openPanel,
      activatePanel,
      closePanel,
      editorGroups,
      activeGroupId,
      focusEditorGroup,
      activateFileInGroup,
      closeFileInGroup,
      reorderFilesInGroup,
      activatePanelInGroup,
      closePanelInGroup,
      splitActiveFile,
      moveFileToGroup,
      splitFileToNewGroup,
      getBuffer,
      writeBuffer,
      isBinaryPath,
    ],
  );

  // file.* / session.* commands + `session` state for window.cremniy.
  // Docs: documentation/architecture/AGENT_CONTROL.md
  const agentValueRef = useRef(value);
  useEffect(() => {
    agentValueRef.current = value;
  }, [value]);

  useEffect(() => {
    const unregisterState = registerAgentState('session', () => {
      const v = agentValueRef.current;
      return {
        activeFilePath: v.activeFilePath,
        openFilePaths: v.openFilePaths,
        dirtyFilePaths: v.dirtyFilePaths,
        activeDocumentDirty: v.activeDocumentDirty,
        activeFileIsBinary: v.activeFileIsBinary,
        documentText: v.documentText,
      };
    });
    const unregisterCommands = registerAgentCommands([
      {
        name: 'session.openFile',
        description: 'Open a workspace file by path { path } (mirrors clicking a file in the tree).',
        run: (args) => agentValueRef.current.openFileFromWorkspace(String(args.path ?? '')),
      },
      {
        name: 'session.activateFile',
        description: 'Focus an already-open tab by path { path }.',
        run: (args) => agentValueRef.current.activateOpenFile(String(args.path ?? '')),
      },
      {
        name: 'session.closeFile',
        description: 'Close an open tab by path { path }.',
        run: (args) => agentValueRef.current.closeOpenFile(String(args.path ?? '')),
      },
      {
        name: 'session.setDocumentText',
        description: 'Replace the active editor text { text } (mirrors typing in the editor).',
        run: (args) => agentValueRef.current.setDocumentText(String(args.text ?? '')),
      },
      {
        name: 'session.splitActiveFile',
        description:
          'Split the active file into a new editor group beside the current one { side?: left | right } (default right). The file opens in both groups, sharing its buffer.',
        run: (args) => {
          const side = args.side === 'left' ? 'left' : 'right';
          agentValueRef.current.splitActiveFile(side);
        },
      },
      {
        name: 'session.moveFileToGroup',
        description:
          'Move an open file tab between editor groups { fromGroupId, toGroupId, path, toIndex? }.',
        run: (args) =>
          agentValueRef.current.moveFileToGroup(
            String(args.fromGroupId ?? ''),
            String(args.toGroupId ?? ''),
            String(args.path ?? ''),
            typeof args.toIndex === 'number' ? args.toIndex : undefined,
          ),
      },
      {
        name: 'session.activateGroup',
        description: 'Focus an editor group by id { groupId } (its tabs/active file become the active group).',
        run: (args) => agentValueRef.current.focusEditorGroup(String(args.groupId ?? '')),
      },
      {
        name: 'file.openFolder',
        description: 'File → Open folder (opens the native folder dialog).',
        run: () => agentValueRef.current.runFileMenuAction('openFolder'),
      },
      {
        name: 'file.openFileDialog',
        description: 'File → Open file (opens the native file dialog).',
        run: () => agentValueRef.current.runFileMenuAction('openFile'),
      },
      {
        name: 'file.save',
        description: 'File → Save the active document.',
        run: () => agentValueRef.current.runFileMenuAction('save'),
      },
      {
        name: 'file.saveAs',
        description: 'File → Save as… the active document.',
        run: () => agentValueRef.current.runFileMenuAction('saveAs'),
      },
      {
        name: 'file.closeEditor',
        description: 'File → Close the active editor tab.',
        run: () => agentValueRef.current.runFileMenuAction('closeEditorTab'),
      },
      {
        name: 'file.closeWorkspace',
        description: 'File → Close workspace (returns to the Welcome screen).',
        run: () => agentValueRef.current.runFileMenuAction('closeWorkspace'),
      },
    ]);
    return () => {
      unregisterState();
      unregisterCommands();
    };
  }, []);

  return <IdeSessionContext.Provider value={value}>{children}</IdeSessionContext.Provider>;
}

export function useIdeSession(): IdeSessionContextValue {
  const v = useContext(IdeSessionContext);
  if (v == null) {
    throw new Error('useIdeSession must be used within IdeSessionProvider');
  }
  return v;
}

/**
 * Like {@link useIdeSession} but returns null instead of throwing when there is
 * no provider. For components that may render outside the IDE shell (e.g. the
 * Extensions panel in a unit test): they degrade gracefully — the openPanel
 * affordance is simply inert — rather than requiring the whole provider tree.
 */
export function useIdeSessionOptional(): IdeSessionContextValue | null {
  return useContext(IdeSessionContext);
}
