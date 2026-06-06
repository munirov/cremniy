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
import type { FileMenuActionId } from '@domain/menu/fileMenu';
import { normalizeFsPath, parentDirectoryPath, fileNameFromPath } from '@domain/workspace/paths';
import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import {
  createProjectFolder,
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
  setDocumentText: (value: string) => void;
  openFileFromWorkspace: (filePath: string) => Promise<void>;
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
  openPanel: (id: string) => void;
  activatePanel: (id: string) => void;
  closePanel: (id: string) => void;
  activateOpenFile: (filePath: string) => void;
  closeOpenFile: (filePath: string) => void;
  closeOtherOpenFiles: (keepFilePath: string) => void;
  closeAllOpenFiles: () => void;
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
  const [openTabs, setOpenTabs] = useState<string[]>([]);
  const [pinnedFilePaths, setPinnedFilePaths] = useState<Set<string>>(() => new Set());
  const [buffers, setBuffers] = useState<Record<string, string>>({});
  const [savedBuffers, setSavedBuffers] = useState<Record<string, string>>({});
  const [activeFilePath, setActiveFilePath] = useState<string | null>(null);
  const [documentText, setDocumentTextState] = useState('');
  const [fileTreeRevision, setFileTreeRevision] = useState(0);
  const [fileContentRevision, setFileContentRevision] = useState(0);
  // Non-file center tabs (e.g. settings) opened in the editor "tab space".
  // activePanel non-null means the center shows that panel instead of the file.
  const [openPanels, setOpenPanels] = useState<string[]>([]);
  const [activePanel, setActivePanel] = useState<string | null>(null);

  const openTabsRef = useRef<string[]>([]);
  const buffersRef = useRef<Record<string, string>>({});
  const savedBuffersRef = useRef<Record<string, string>>({});
  // UTF-8 BOM preservation: each open path remembers whether its on-disk
  // bytes started with EF BB BF, so save() can re-emit the BOM.
  const bomFlagsRef = useRef<Record<string, boolean>>({});

  useEffect(() => {
    openTabsRef.current = openTabs;
  }, [openTabs]);

  useEffect(() => {
    buffersRef.current = buffers;
  }, [buffers]);

  useEffect(() => {
    savedBuffersRef.current = savedBuffers;
  }, [savedBuffers]);

  const bumpFileTreeRevision = useCallback(() => {
    setFileTreeRevision((n) => n + 1);
  }, []);
  const bumpFileContentRevision = useCallback(() => {
    setFileContentRevision((n) => n + 1);
  }, []);

  const togglePinFilePath = useCallback((filePath: string) => {
    setPinnedFilePaths((prev) => {
      const next = new Set(prev);
      if (next.has(filePath)) next.delete(filePath);
      else next.add(filePath);
      return next;
    });
  }, []);

  // Swap two tabs. Caller must ensure both indices live in the same zone
  // (both pinned or both unpinned) — the strip enforces that.
  const reorderOpenFiles = useCallback((fromIndex: number, toIndex: number) => {
    setOpenTabs((tabs) => {
      if (
        fromIndex < 0 ||
        toIndex < 0 ||
        fromIndex >= tabs.length ||
        toIndex >= tabs.length ||
        fromIndex === toIndex
      ) {
        return tabs;
      }
      const next = tabs.slice();
      const [moved] = next.splice(fromIndex, 1);
      next.splice(toIndex, 0, moved!);
      return next;
    });
  }, []);

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

  const activateOpenFile = useCallback((filePath: string) => {
    const path = filePath.trim();
    if (path === '') {
      return;
    }
    if (!openTabsRef.current.includes(path)) {
      return;
    }
    setActivePanel(null);
    setActiveFilePath(path);
    setDocumentTextState(buffersRef.current[path] ?? '');
  }, []);

  const closeOpenFile = useCallback(
    (filePath: string) => {
      const path = filePath.trim();
      if (path === '') {
        return;
      }

      const currentBuffer = buffersRef.current[path];
      const saved = savedBuffersRef.current[path];
      if ((currentBuffer ?? '') !== (saved ?? '')) {
        const name = fileNameFromPath(path) || path;
        if (!window.confirm(`Discard unsaved changes in "${name}"?`)) {
          return;
        }
      }

      const tabsBefore = openTabsRef.current;
      const nextTabs = tabsBefore.filter((t) => t !== path);
      const nextBuffers = { ...buffersRef.current };
      const nextSavedBuffers = { ...savedBuffersRef.current };
      delete nextBuffers[path];
      delete nextSavedBuffers[path];

      setOpenTabs(nextTabs);
      setBuffers(nextBuffers);
      setSavedBuffers(nextSavedBuffers);
      setPinnedFilePaths((prev) => {
        if (!prev.has(path)) return prev;
        const next = new Set(prev);
        next.delete(path);
        return next;
      });

      if (activeFilePath !== path) {
        return;
      }

      if (nextTabs.length === 0) {
        setActiveFilePath(null);
        setDocumentTextState('');
        return;
      }

      const oldIdx = tabsBefore.indexOf(path);
      const nextIdx = oldIdx > 0 ? oldIdx - 1 : 0;
      const nextPath = nextTabs[nextIdx] ?? nextTabs[0];
      setActiveFilePath(nextPath);
      setDocumentTextState(nextBuffers[nextPath] ?? '');
    },
    [activeFilePath],
  );

  const closeOtherOpenFiles = useCallback(
    (keepFilePath: string) => {
      const keep = keepFilePath.trim();
      const tabs = openTabsRef.current.slice();
      for (const t of tabs) {
        if (t !== keep && !pinnedFilePaths.has(t)) {
          closeOpenFile(t);
        }
      }
    },
    [closeOpenFile, pinnedFilePaths],
  );

  const closeAllOpenFiles = useCallback(() => {
    const tabs = openTabsRef.current.slice();
    for (const t of tabs) {
      if (!pinnedFilePaths.has(t)) {
        closeOpenFile(t);
      }
    }
  }, [closeOpenFile, pinnedFilePaths]);

  const setDocumentText = useCallback(
    (value: string) => {
      setDocumentTextState(value);
      if (activeFilePath != null && activeFilePath !== '') {
        setBuffers((prev) => ({ ...prev, [activeFilePath]: value }));
      }
    },
    [activeFilePath],
  );

  const openWorkspaceFolderFlow = useCallback(async () => {
    const path = await pickFolder();
    if (path == null || path === '') {
      return;
    }
    setOpenTabs([]);
    setBuffers({});
    setSavedBuffers({});
    setActiveFilePath(null);
    setDocumentTextState('');
    await persistRecentAndNavigate(path);
  }, [persistRecentAndNavigate]);

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
      setOpenTabs([]);
      setBuffers({});
      setSavedBuffers({});
      setActiveFilePath(null);
      setDocumentTextState('');
      await persistRecentAndNavigate(created);
    } catch (e) {
      notify.error('Could not create project', formatUserMessage(e));
    }
  }, [notify, persistRecentAndNavigate]);

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
      setOpenTabs([]);
      setBuffers({});
      setSavedBuffers({});
      setActiveFilePath(null);
      setDocumentTextState('');
    }
    await persistRecentAndNavigate(parent);
    if (openTabsRef.current.includes(path)) {
      activateOpenFile(path);
      return;
    }
    setOpenTabs((t) => [...t, path]);
    setBuffers((b) => ({ ...b, [path]: text }));
    setSavedBuffers((b) => ({ ...b, [path]: text }));
    setActiveFilePath(path);
    setDocumentTextState(text);
  }, [activateOpenFile, persistRecentAndNavigate, workspaceRoot?.path]);

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
      setOpenTabs((tabs) => {
        const mergeWithExistingChosenTab =
          chosen !== oldPath && tabs.includes(chosen);
        if (mergeWithExistingChosenTab) {
          return tabs.filter((t) => t !== oldPath);
        }
        return tabs.map((t) => (t === oldPath ? chosen : t));
      });
      setBuffers((prev) => {
        const next = { ...prev };
        delete next[oldPath];
        next[chosen] = documentText;
        return next;
      });
      setSavedBuffers((prev) => {
        const next = { ...prev };
        if (chosen !== oldPath) {
          delete next[oldPath];
        }
        next[chosen] = documentText;
        return next;
      });
    } else {
      setOpenTabs((tabs) => (tabs.includes(chosen) ? tabs : [...tabs, chosen]));
      setBuffers((prev) => ({ ...prev, [chosen]: documentText }));
      setSavedBuffers((prev) => ({ ...prev, [chosen]: documentText }));
    }
    setActiveFilePath(chosen);
  }, [activeFilePath, documentText]);

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

  const openFileFromWorkspace = useCallback(
    async (filePath: string) => {
      const trimmed = filePath.trim();
      if (trimmed === '') {
        return;
      }
      const rootPath = workspaceRoot?.path?.trim() ?? '';
      if (rootPath === '') {
        notify.warn('No workspace is open. Open a folder from the File menu first.');
        return;
      }
      if (openTabsRef.current.includes(trimmed)) {
        activateOpenFile(trimmed);
        return;
      }
      try {
        const raw = await readWorkspaceUserFile(rootPath, trimmed);
        // Detect UTF-8 BOM (0xFEFF in JS string) so we can re-emit it on save.
        const hasBom = raw.charCodeAt(0) === 0xfeff;
        const text = hasBom ? raw.slice(1) : raw;
        bomFlagsRef.current = { ...bomFlagsRef.current, [trimmed]: hasBom };
        setOpenTabs((t) => [...t, trimmed]);
        setBuffers((b) => ({ ...b, [trimmed]: text }));
        setSavedBuffers((b) => ({ ...b, [trimmed]: text }));
        setActivePanel(null);
        setActiveFilePath(trimmed);
        setDocumentTextState(text);
      } catch (e) {
        notify.error('Could not open file', formatUserMessage(e));
      }
    },
    [notify, workspaceRoot?.path, activateOpenFile],
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
    const clean = openTabsRef.current.filter(
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
        if (activeFilePath === p) {
          setDocumentTextState(text);
        }
      } catch {
        // File may have been deleted/renamed — skip it.
      }
    }
    bumpFileContentRevision();
  }, [workspaceRoot?.path, activeFilePath, bumpFileContentRevision]);

  const openPanel = useCallback((id: string) => {
    setOpenPanels((prev) => (prev.includes(id) ? prev : [...prev, id]));
    setActivePanel(id);
  }, []);

  const activatePanel = useCallback((id: string) => {
    setActivePanel(id);
  }, []);

  const closePanel = useCallback((id: string) => {
    setOpenPanels((prev) => prev.filter((p) => p !== id));
    setActivePanel((prev) => (prev === id ? null : prev));
  }, []);

  const closeWorkspaceFlow = useCallback(() => {
    setOpenTabs([]);
    setBuffers({});
    setSavedBuffers({});
    setActiveFilePath(null);
    setDocumentTextState('');
    navigate('/', { replace: true });
  }, [navigate]);

  const closeEditorTabFlow = useCallback(() => {
    const tabs = openTabsRef.current;
    const path = activeFilePath?.trim() ?? '';

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
  }, [activeFilePath, closeOpenFile, documentText]);

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
      openPanel,
      activatePanel,
      closePanel,
    }),
    [
      activeDocumentDirty,
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
      openPanel,
      activatePanel,
      closePanel,
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
