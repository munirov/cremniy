import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { registerAgentCommands, registerAgentState } from '@shared/agent/agentBridge';
import type { ToolTabId } from '@domain/toolTabs/toolTabId';
import { AgentWorkspaceCommands } from '@boundary/agent/AgentWorkspaceCommands';

import type { EditMenuActionId } from '@domain/menu/editMenu';
import type { FileMenuActionId } from '@domain/menu/fileMenu';
import type { GlobalShortcutAction } from '@domain/menu/menuShortcuts';
import type { ReferencesMenuActionId } from '@domain/menu/referencesMenu';
import type { TerminalMenuActionId } from '@domain/menu/terminalMenu';
import type { ToolsMenuActionId } from '@domain/menu/toolsMenu';
import type { ViewMenuActionId } from '@domain/menu/viewMenu';
import { DEFAULT_APP_PREFERENCES, type AppPreferences } from '@domain/preferences/appPreferences';
import type { SettingsService } from '@domain/preferences/settingsService';

import type { IdeEditorCommand, IdeEditorCursorPosition } from '@boundary/editor/IdeMonacoEditor';
import { MenuBar } from '@boundary/chrome/MenuBar';
import { useMenuSlot } from '@boundary/chrome/MenuSlotContext';
import { useChangeLocale } from '@boundary/i18n/LocaleContext';
import { ReferenceTablesModal } from '@boundary/references/ReferenceTablesModal';
import { SettingsDialog } from '@boundary/settings/SettingsDialog';
import { SettingsApplyProvider } from '@boundary/layout/centerPanels';
import { IdeDockview } from '@boundary/layout/IdeDockview';
import { RootLayout } from '@boundary/layout/RootLayout';
import { DataConverterDialog } from '@boundary/tools/DataConverterDialog';
import { ReverseCalculatorDialog } from '@boundary/tools/ReverseCalculatorDialog';
import { ShellCodeGeneratorDialog } from '@boundary/tools/ShellCodeGeneratorDialog';
import { IdeSessionProvider, useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { ToolDockProvider, useToolDock } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';

import styles from './RootApp.module.css';

type RootAppProps = {
  settingsService: SettingsService;
};

function RootAppIdeShell({ settingsService }: RootAppProps) {
  const workspaceRoot = useWorkspaceRoot();
  const ide = useIdeSession();
  const toolDock = useToolDock();
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [calcOpen, setCalcOpen] = useState(false);
  const [dataConverterOpen, setDataConverterOpen] = useState(false);
  const [shellCodeOpen, setShellCodeOpen] = useState(false);
  const [refsOpen, setRefsOpen] = useState<ReferencesMenuActionId | null>(null);
  // Bumped by Terminal → New Terminal to ask the panel to spawn a tab.
  const [newTerminalSignal, setNewTerminalSignal] = useState(0);
  const [prefs, setPrefs] = useState<AppPreferences | null>(null);
  const [cursorPosition, setCursorPosition] = useState<IdeEditorCursorPosition | null>(null);
  const [editorCommand, setEditorCommand] = useState<IdeEditorCommand | null>(null);
  const [hiddenPanes, setHiddenPanes] = useState<Record<'fileTree' | 'editor' | 'toolDock', boolean>>({
    fileTree: false,
    editor: false,
    toolDock: false,
  });

  // togglePane defined further down — it needs persistTerminal in scope.
  const togglePaneRef = useRef<(id: 'fileTree' | 'editor' | 'toolDock' | 'terminal') => void>(() => undefined);

  useEffect(() => {
    let cancelled = false;
    void settingsService.loadPreferences().then(
      (p) => {
        if (!cancelled) {
          setPrefs(p);
        }
      },
      () => {
        if (!cancelled) {
          setPrefs(DEFAULT_APP_PREFERENCES);
        }
      },
    );
    return () => {
      cancelled = true;
    };
  }, [settingsService]);

  const changeLocale = useChangeLocale();
  useEffect(() => {
    if (prefs?.locale != null) changeLocale(prefs.locale);
  }, [prefs?.locale, changeLocale]);

  const terminalVisible = prefs?.terminalPanelVisible ?? false;

  const wordWrapEnabled =
    prefs?.editorWordWrap ?? DEFAULT_APP_PREFERENCES.editorWordWrap;
  const editorFontSize = prefs?.editorFontSize ?? DEFAULT_APP_PREFERENCES.editorFontSize;

  const persistEditorFontSize = useCallback(
    (size: number) => {
      const base = prefsRef.current ?? DEFAULT_APP_PREFERENCES;
      if (base.editorFontSize === size) {
        return;
      }
      const next: AppPreferences = { ...base, editorFontSize: size };
      void settingsService.savePreferences(next).catch(() => undefined);
      setPrefs(next);
    },
    [settingsService],
  );

  const persistTerminal = useCallback(async (visible: boolean) => {
    const base = prefs ?? (await settingsService.loadPreferences().catch(() => DEFAULT_APP_PREFERENCES));
    const next: AppPreferences = { ...base, terminalPanelVisible: visible };
    await settingsService.savePreferences(next);
    setPrefs(next);
  }, [prefs, settingsService]);

  const persistWordWrap = useCallback(
    async (enabled: boolean) => {
      const base = prefs ?? (await settingsService.loadPreferences().catch(() => DEFAULT_APP_PREFERENCES));
      const next: AppPreferences = { ...base, editorWordWrap: enabled };
      await settingsService.savePreferences(next);
      setPrefs(next);
    },
    [prefs, settingsService],
  );

  const persistInsertSpaces = useCallback(
    async (enabled: boolean) => {
      const base = prefs ?? (await settingsService.loadPreferences().catch(() => DEFAULT_APP_PREFERENCES));
      const next: AppPreferences = { ...base, editorInsertSpaces: enabled };
      await settingsService.savePreferences(next);
      setPrefs(next);
    },
    [prefs, settingsService],
  );

  const persistTabWidth = useCallback(
    async (width: number) => {
      const base = prefs ?? (await settingsService.loadPreferences().catch(() => DEFAULT_APP_PREFERENCES));
      const next: AppPreferences = { ...base, editorTabWidth: width };
      await settingsService.savePreferences(next);
      setPrefs(next);
    },
    [prefs, settingsService],
  );

  // Fire-and-forget layout persistence — IdeDockview already debounces it.
  const prefsRef = useRef(prefs);
  prefsRef.current = prefs;
  const persistDockLayout = useCallback(
    (layout: unknown) => {
      const base = prefsRef.current ?? DEFAULT_APP_PREFERENCES;
      const next: AppPreferences = { ...base, dockLayout: layout };
      void settingsService.savePreferences(next).catch(() => undefined);
      setPrefs(next);
    },
    [settingsService],
  );

  // Bind the dock context-menu's togglePane to the live persistTerminal closure.
  togglePaneRef.current = (id) => {
    if (id === 'terminal') {
      void persistTerminal(!terminalVisible);
      return;
    }
    setHiddenPanes((prev) => ({ ...prev, [id]: !prev[id] }));
  };
  const onTogglePane = useCallback(
    (id: 'fileTree' | 'editor' | 'toolDock' | 'terminal') => togglePaneRef.current(id),
    [],
  );

  const editorInsertSpaces =
    prefs?.editorInsertSpaces ?? DEFAULT_APP_PREFERENCES.editorInsertSpaces;
  const editorTabWidth = prefs?.editorTabWidth ?? DEFAULT_APP_PREFERENCES.editorTabWidth;

  const handleViewMenu = useCallback(
    (id: ViewMenuActionId) => {
      if (id === 'toggleTerminal') {
        void persistTerminal(!terminalVisible);
        return;
      }
      if (id === 'toggleWordWrap') {
        void persistWordWrap(!wordWrapEnabled);
        return;
      }
      if (id === 'toggleFileTree') {
        setHiddenPanes((p) => ({ ...p, fileTree: !p.fileTree }));
        return;
      }
      if (id === 'toggleInsertSpaces') {
        void persistInsertSpaces(!editorInsertSpaces);
        return;
      }
      if (id === 'setTabWidth2') {
        void persistTabWidth(2);
        return;
      }
      if (id === 'setTabWidth4') {
        void persistTabWidth(4);
        return;
      }
      if (id === 'setTabWidth8') {
        void persistTabWidth(8);
      }
    },
    [
      editorInsertSpaces,
      persistInsertSpaces,
      persistTabWidth,
      persistTerminal,
      persistWordWrap,
      terminalVisible,
      wordWrapEnabled,
    ],
  );

  const handleEditMenu = useCallback((id: EditMenuActionId) => {
    if (id === 'findInEditor') {
      setEditorCommand({ kind: 'findInEditor', nonce: Date.now() });
    }
  }, []);

  const handleToolsMenu = useCallback(
    (id: ToolsMenuActionId) => {
      if (id === 'reverseCalculator') {
        setCalcOpen(true);
        return;
      }
      if (id === 'dataConverter') {
        setDataConverterOpen(true);
        return;
      }
      if (id === 'shellCodeGenerator') {
        setShellCodeOpen(true);
        return;
      }
      if (id === 'openBinaryTool') {
        toolDock.setActiveToolTab('binary');
        return;
      }
      if (id === 'openCodeEditorTool') {
        toolDock.setActiveToolTab('codeEditor');
        return;
      }
      if (id === 'openDisassemblerTool') {
        toolDock.setActiveToolTab('disassembler');
      }
    },
    [toolDock],
  );

  const handleRefsMenu = useCallback((id: ReferencesMenuActionId) => {
    setRefsOpen(id);
  }, []);

  const handleTerminalMenu = useCallback(
    (id: TerminalMenuActionId) => {
      if (id === 'newTerminal') {
        void persistTerminal(true);
        setNewTerminalSignal((s) => s + 1);
      }
    },
    [persistTerminal],
  );

  // Collapse the terminal panel (used by its own hide / close buttons).
  const hideTerminal = useCallback(() => {
    void persistTerminal(false);
  }, [persistTerminal]);

  const onFileMenuAction = useMemo(
    () => (id: FileMenuActionId) => {
      if (id === 'preferences') {
        setSettingsOpen(true);
        return;
      }
      void ide.runFileMenuAction(id);
    },
    [ide],
  );

  const onShortcut = useCallback(
    (action: GlobalShortcutAction) => {
      if (action.kind === 'file') {
        if (action.id === 'preferences') {
          setSettingsOpen(true);
          return;
        }
        void ide.runFileMenuAction(action.id);
        return;
      }
      if (action.kind === 'edit') {
        handleEditMenu(action.id);
        return;
      }
      if (action.kind === 'view') {
        handleViewMenu(action.id);
        return;
      }
    },
    [handleEditMenu, handleViewMenu, ide],
  );

  const onIdeCursorPositionChange = useCallback((position: IdeEditorCursorPosition | null) => {
    setCursorPosition(position);
  }, []);


  const closeEditorAvailable =
    ide.openFilePaths.length > 0 || ide.documentText.trim() !== '';

  // IDE chrome commands + `ui` state for window.cremniy. The ref lets the
  // registration effect run once while still reading current values.
  // Docs: documentation/architecture/AGENT_CONTROL.md
  const agentUiRef = useRef({
    workspaceRoot,
    toolDock,
    terminalVisible,
    wordWrapEnabled,
    settingsOpen,
    openPanels: ide.openPanels,
    calcOpen,
    refsOpen,
    handleViewMenu,
    handleToolsMenu,
    setSettingsOpen,
    openPanel: ide.openPanel,
    closePanel: ide.closePanel,
    setCalcOpen,
    setRefsOpen,
    setEditorCommand,
  });
  useEffect(() => {
    agentUiRef.current = {
      workspaceRoot,
      toolDock,
      terminalVisible,
      wordWrapEnabled,
      settingsOpen,
      openPanels: ide.openPanels,
      calcOpen,
      refsOpen,
      handleViewMenu,
      handleToolsMenu,
      setSettingsOpen,
      openPanel: ide.openPanel,
      closePanel: ide.closePanel,
      setCalcOpen,
      setRefsOpen,
      setEditorCommand,
    };
  });

  useEffect(() => {
    const VALID_TOOL_TABS: readonly ToolTabId[] = ['binary', 'codeEditor', 'disassembler'];
    const toToolTabId = (value: unknown): ToolTabId => {
      if (typeof value === 'string' && (VALID_TOOL_TABS as readonly string[]).includes(value)) {
        return value as ToolTabId;
      }
      throw new Error(`Invalid tool tab id. Expected one of: ${VALID_TOOL_TABS.join(', ')}.`);
    };

    const unregisterState = registerAgentState('ui', () => {
      const u = agentUiRef.current;
      return {
        route: 'ide',
        workspaceRoot: u.workspaceRoot?.path ?? null,
        activeToolTab: u.toolDock.activeToolTab,
        terminalPanelVisible: u.terminalVisible,
        editorWordWrap: u.wordWrapEnabled,
        openDialogs: {
          settings: u.settingsOpen,
          advancedGit: u.openPanels?.includes('advancedGit') ?? false,
          reverseCalculator: u.calcOpen,
          reference: u.refsOpen,
        },
      };
    });

    const unregisterCommands = registerAgentCommands([
      {
        name: 'view.toggleTerminal',
        description: 'View → toggle the terminal panel.',
        run: () => agentUiRef.current.handleViewMenu('toggleTerminal'),
      },
      {
        name: 'view.toggleWordWrap',
        description: 'View → toggle editor word wrap.',
        run: () => agentUiRef.current.handleViewMenu('toggleWordWrap'),
      },
      {
        name: 'tool.select',
        description: 'Select a tool panel { id: binary | codeEditor | disassembler }.',
        run: (args) => agentUiRef.current.toolDock.setActiveToolTab(toToolTabId(args.id)),
      },
      {
        name: 'edit.find',
        description: 'Edit → Find in the active editor (opens Monaco find).',
        run: () => agentUiRef.current.setEditorCommand({ kind: 'findInEditor', nonce: Date.now() }),
      },
      {
        name: 'dialog.openSettings',
        description: 'Open the Preferences dialog (Edit → Settings).',
        run: () => agentUiRef.current.setSettingsOpen(true),
      },
      {
        name: 'dialog.openAdvancedGit',
        description: 'Open the Advanced Git panel (branches, merge, rebase, stash, history, remotes) as a center tab.',
        run: () => agentUiRef.current.openPanel('advancedGit'),
      },
      {
        name: 'dialog.openReverseCalculator',
        description: 'Open the Reverse Calculator dialog (Tools).',
        run: () => agentUiRef.current.setCalcOpen(true),
      },
      {
        name: 'dialog.openReference',
        description: 'Open a References table { kind: asciiTable | scancodeTable }.',
        run: (args) => {
          const kind = args.kind;
          if (kind !== 'asciiTable' && kind !== 'scancodeTable') {
            throw new Error('Invalid reference kind. Expected: asciiTable | scancodeTable.');
          }
          agentUiRef.current.setRefsOpen(kind);
        },
      },
      {
        name: 'dialog.closeAll',
        description: 'Close any open Settings / Advanced Git / Reverse Calculator / References dialog.',
        run: () => {
          agentUiRef.current.setSettingsOpen(false);
          agentUiRef.current.closePanel('advancedGit');
          agentUiRef.current.setCalcOpen(false);
          agentUiRef.current.setRefsOpen(null);
        },
      },
    ]);

    return () => {
      unregisterState();
      unregisterCommands();
    };
  }, []);

  // Publish the IDE's MenuBar into the global TitleBar (which lives one level
  // above the routes in App.tsx — see MenuSlotContext for the rationale).
  // Clearing on unmount means navigating back to Welcome hides the menu items
  // but the titlebar / window controls stay visible.
  const { setMenu, setSettingsAction } = useMenuSlot();
  // The titlebar gear opens settings as a center tab (the generic tab space),
  // not a modal.
  const openCenterPanel = ide.openPanel;
  useEffect(() => {
    setSettingsAction(() => openCenterPanel('settings'));
    return () => setSettingsAction(null);
  }, [setSettingsAction, openCenterPanel]);
  useEffect(() => {
    setMenu(
      <MenuBar
        onFileMenuAction={onFileMenuAction}
        onEditMenuAction={handleEditMenu}
        onViewMenuAction={handleViewMenu}
        onToolsMenuAction={handleToolsMenu}
        onReferencesMenuAction={handleRefsMenu}
        onTerminalMenuAction={handleTerminalMenu}
        onShortcut={onShortcut}
        terminalPanelVisible={terminalVisible}
        wordWrapEnabled={wordWrapEnabled}
        hasActiveDocument={ide.activeFilePath != null || ide.documentText !== ''}
        activeDocumentDirty={ide.activeDocumentDirty}
        closeEditorAvailable={closeEditorAvailable}
      />,
    );
    return () => setMenu(null);
  }, [
    closeEditorAvailable,
    handleEditMenu,
    handleRefsMenu,
    handleTerminalMenu,
    handleToolsMenu,
    handleViewMenu,
    ide.activeDocumentDirty,
    ide.activeFilePath,
    ide.documentText,
    onFileMenuAction,
    onShortcut,
    setMenu,
    terminalVisible,
    wordWrapEnabled,
  ]);

  return (
    <>
      <SettingsApplyProvider apply={setPrefs}>
      <RootLayout>
        <IdeDockview
          workspaceRoot={workspaceRoot}
          editorCommand={editorCommand}
          wordWrapEnabled={wordWrapEnabled}
          editorInsertSpaces={editorInsertSpaces}
          editorTabWidth={editorTabWidth}
          editorFontSize={editorFontSize}
          onEditorFontSizeChange={persistEditorFontSize}
          onCursorPositionChange={onIdeCursorPositionChange}
          cursorPosition={cursorPosition}
          initialLayout={prefs?.dockLayout ?? null}
          onLayoutChange={persistDockLayout}
          terminalVisible={terminalVisible}
          newTerminalSignal={newTerminalSignal}
          onHideTerminal={hideTerminal}
          onTogglePane={onTogglePane}
          paneVisibility={{
            fileTree: !hiddenPanes.fileTree,
            editor: !hiddenPanes.editor,
            toolDock: !hiddenPanes.toolDock,
            terminal: terminalVisible,
          }}
        />
      </RootLayout>
      </SettingsApplyProvider>

      <SettingsDialog
        open={settingsOpen}
        onClose={() => setSettingsOpen(false)}
        onSaved={(next) => setPrefs(next)}
        workspaceRoot={workspaceRoot?.path ?? null}
        service={settingsService}
      />

      {calcOpen ? (
        <div className={styles.modalBackdrop} role="presentation" onMouseDown={(e) => e.target === e.currentTarget && setCalcOpen(false)}>
          <div className={styles.modalPanel} onMouseDown={(e) => e.stopPropagation()}>
            <ReverseCalculatorDialog onClose={() => setCalcOpen(false)} />
          </div>
        </div>
      ) : null}

      {dataConverterOpen ? (
        <div
          className={styles.modalBackdrop}
          role="presentation"
          onMouseDown={(e) => e.target === e.currentTarget && setDataConverterOpen(false)}
        >
          <div className={styles.modalPanel} onMouseDown={(e) => e.stopPropagation()}>
            <DataConverterDialog onClose={() => setDataConverterOpen(false)} />
          </div>
        </div>
      ) : null}

      {shellCodeOpen ? (
        <div
          className={styles.modalBackdrop}
          role="presentation"
          onMouseDown={(e) => e.target === e.currentTarget && setShellCodeOpen(false)}
        >
          <div className={styles.modalPanel} onMouseDown={(e) => e.stopPropagation()}>
            <ShellCodeGeneratorDialog onClose={() => setShellCodeOpen(false)} />
          </div>
        </div>
      ) : null}

      {refsOpen != null ? <ReferenceTablesModal kind={refsOpen} onClose={() => setRefsOpen(null)} /> : null}

      <AgentWorkspaceCommands />
    </>
  );
}

export function RootApp({ settingsService }: RootAppProps) {
  return (
    <IdeSessionProvider>
      <ToolDockProvider>
        <RootAppIdeShell settingsService={settingsService} />
      </ToolDockProvider>
    </IdeSessionProvider>
  );
}
