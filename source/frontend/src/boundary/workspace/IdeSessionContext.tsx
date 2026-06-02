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
  pickFile,
  pickFolder,
  pickSaveFile,
  readUserFile,
  readWorkspaceUserFile,
  writeUserFile,
} from '@infrastructure/tauri/bridge';

import { registerAgentCommands, registerAgentState } from '@shared/agent/agentBridge';

import { useWorkspaceRoot } from './WorkspaceContext';

export type IdeSessionContextValue = {
  activeFilePath: string | null;
  openFilePaths: string[];
  documentText: string;
  dirtyFilePaths: string[];
  activeDocumentDirty: boolean;
  setDocumentText: (value: string) => void;
  openFileFromWorkspace: (filePath: string) => Promise<void>;
  activateOpenFile: (filePath: string) => void;
  closeOpenFile: (filePath: string) => void;
  runFileMenuAction: (id: FileMenuActionId) => Promise<void>;
  fileTreeRevision: number;
  bumpFileTreeRevision: () => void;
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
  const [openTabs, setOpenTabs] = useState<string[]>([]);
  const [buffers, setBuffers] = useState<Record<string, string>>({});
  const [savedBuffers, setSavedBuffers] = useState<Record<string, string>>({});
  const [activeFilePath, setActiveFilePath] = useState<string | null>(null);
  const [documentText, setDocumentTextState] = useState('');
  const [fileTreeRevision, setFileTreeRevision] = useState(0);

  const openTabsRef = useRef<string[]>([]);
  const buffersRef = useRef<Record<string, string>>({});
  const savedBuffersRef = useRef<Record<string, string>>({});

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

  const openFileFlow = useCallback(async () => {
    const path = await pickFile();
    if (path == null || path === '') {
      return;
    }
    const parent = parentDirectoryPath(path);
    if (parent === '') {
      window.alert('Could not determine folder for the selected file.');
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
    await writeUserFile(chosen, documentText);
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
    await writeUserFile(activeFilePath, documentText);
    setSavedBuffers((prev) => ({ ...prev, [activeFilePath]: documentText }));
  }, [activeFilePath, documentText, saveDocumentAsFlow]);

  const openFileFromWorkspace = useCallback(
    async (filePath: string) => {
      const trimmed = filePath.trim();
      if (trimmed === '') {
        return;
      }
      const rootPath = workspaceRoot?.path?.trim() ?? '';
      if (rootPath === '') {
        window.alert('No workspace is open. Open a folder from the File menu first.');
        return;
      }
      if (openTabsRef.current.includes(trimmed)) {
        activateOpenFile(trimmed);
        return;
      }
      try {
        const text = await readWorkspaceUserFile(rootPath, trimmed);
        setOpenTabs((t) => [...t, trimmed]);
        setBuffers((b) => ({ ...b, [trimmed]: text }));
        setSavedBuffers((b) => ({ ...b, [trimmed]: text }));
        setActiveFilePath(trimmed);
        setDocumentTextState(text);
      } catch (e) {
        window.alert(formatUserMessage(e));
      }
    },
    [workspaceRoot?.path, activateOpenFile],
  );

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
        window.alert(formatUserMessage(e));
      }
    },
    [
      closeEditorTabFlow,
      closeWorkspaceFlow,
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
      documentText,
      dirtyFilePaths,
      activeDocumentDirty,
      setDocumentText,
      openFileFromWorkspace,
      activateOpenFile,
      closeOpenFile,
      runFileMenuAction,
      fileTreeRevision,
      bumpFileTreeRevision,
    }),
    [
      activeDocumentDirty,
      activeFilePath,
      dirtyFilePaths,
      openTabs,
      documentText,
      setDocumentText,
      openFileFromWorkspace,
      activateOpenFile,
      closeOpenFile,
      runFileMenuAction,
      fileTreeRevision,
      bumpFileTreeRevision,
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
