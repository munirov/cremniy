import { useCallback, useEffect, useMemo, useRef, useState } from 'react';

import { registerAgentCommands, registerAgentState } from '@shared/agent/agentBridge';
import type { ToolTabId } from '@domain/toolTabs/toolTabId';
import { AgentWorkspaceCommands } from '@boundary/agent/AgentWorkspaceCommands';

import type { EditMenuActionId } from '@domain/menu/editMenu';
import type { FileMenuActionId } from '@domain/menu/fileMenu';
import type { GlobalShortcutAction } from '@domain/menu/menuShortcuts';
import type { ReferencesMenuActionId } from '@domain/menu/referencesMenu';
import type { ToolsMenuActionId } from '@domain/menu/toolsMenu';
import type { ViewMenuActionId } from '@domain/menu/viewMenu';
import { DEFAULT_APP_PREFERENCES, type AppPreferences } from '@domain/preferences/appPreferences';
import type { SettingsService } from '@domain/preferences/settingsService';

import type { IdeEditorCommand, IdeEditorCursorPosition } from '@boundary/editor/IdeMonacoEditor';
import { IdeMonacoEditor } from '@boundary/editor/IdeMonacoEditor';
import { MenuBar } from '@boundary/chrome/MenuBar';
import { ReferenceTablesModal } from '@boundary/references/ReferenceTablesModal';
import { SettingsDialog } from '@boundary/settings/SettingsDialog';
import { TerminalFooterPanel } from '@boundary/terminal/TerminalFooterPanel';
import { ReverseCalculatorDialog } from '@boundary/tools/ReverseCalculatorDialog';
import { IdeStatusStrip } from '@boundary/layout/IdeStatusStrip';
import { IdeWorkspace } from '@boundary/layout/IdeWorkspace';
import { RootLayout } from '@boundary/layout/RootLayout';
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
  const [refsOpen, setRefsOpen] = useState<ReferencesMenuActionId | null>(null);
  const [prefs, setPrefs] = useState<AppPreferences | null>(null);
  const [cursorPosition, setCursorPosition] = useState<IdeEditorCursorPosition | null>(null);
  const [editorCommand, setEditorCommand] = useState<IdeEditorCommand | null>(null);

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

  const terminalVisible = prefs?.terminalPanelVisible ?? false;

  const wordWrapEnabled =
    prefs?.editorWordWrap ?? DEFAULT_APP_PREFERENCES.editorWordWrap;

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

  const handleViewMenu = useCallback(
    (id: ViewMenuActionId) => {
      if (id === 'toggleTerminal') {
        void persistTerminal(!terminalVisible);
        return;
      }
      if (id === 'toggleWordWrap') {
        void persistWordWrap(!wordWrapEnabled);
      }
    },
    [persistTerminal, persistWordWrap, terminalVisible, wordWrapEnabled],
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
      if (action.kind === 'view' && action.id === 'toggleTerminal') {
        void persistTerminal(!terminalVisible);
        return;
      }
      if (action.kind === 'view' && action.id === 'toggleWordWrap') {
        void persistWordWrap(!wordWrapEnabled);
      }
    },
    [handleEditMenu, ide, persistTerminal, persistWordWrap, terminalVisible, wordWrapEnabled],
  );

  const onCloseWorkspace = useMemo(
    () => () => {
      void ide.runFileMenuAction('closeWorkspace');
    },
    [ide],
  );

  const onIdeCursorPositionChange = useCallback((position: IdeEditorCursorPosition | null) => {
    setCursorPosition(position);
  }, []);

  const footerPanel = useMemo(
    () => <TerminalFooterPanel workspaceRoot={workspaceRoot?.path ?? null} />,
    [workspaceRoot?.path],
  );

  const closeEditorAvailable =
    ide.openFilePaths.length > 0 || ide.documentText.trim() !== '';

  // IDE chrome commands + `ui` state for window.cremniy. The ref lets the
  // registration effect run once while still reading current values.
  // Docs: documentation/EN/agent_control_surface.md
  const agentUiRef = useRef({
    workspaceRoot,
    toolDock,
    terminalVisible,
    wordWrapEnabled,
    settingsOpen,
    calcOpen,
    refsOpen,
    handleViewMenu,
    handleToolsMenu,
    setSettingsOpen,
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
      calcOpen,
      refsOpen,
      handleViewMenu,
      handleToolsMenu,
      setSettingsOpen,
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
        description: 'Close any open Settings / Reverse Calculator / References dialog.',
        run: () => {
          agentUiRef.current.setSettingsOpen(false);
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

  return (
    <>
      <RootLayout
        header={
          <MenuBar
            onFileMenuAction={onFileMenuAction}
            onEditMenuAction={handleEditMenu}
            onViewMenuAction={handleViewMenu}
            onToolsMenuAction={handleToolsMenu}
            onReferencesMenuAction={handleRefsMenu}
            onShortcut={onShortcut}
            terminalPanelVisible={terminalVisible}
            wordWrapEnabled={wordWrapEnabled}
            hasActiveDocument={ide.activeFilePath != null || ide.documentText !== ''}
            activeDocumentDirty={ide.activeDocumentDirty}
            closeEditorAvailable={closeEditorAvailable}
          />
        }
        footerVisible={terminalVisible}
        footerPanel={footerPanel}
      >
        <IdeWorkspace workspaceRoot={workspaceRoot} onCloseWorkspace={onCloseWorkspace}>
          <div className={styles.ideEditorPane}>
            <IdeMonacoEditor
              onCursorPositionChange={onIdeCursorPositionChange}
              wordWrapEnabled={wordWrapEnabled}
              command={editorCommand}
            />
            <IdeStatusStrip
              activeFilePath={ide.activeFilePath}
              cursorLine={cursorPosition?.line ?? null}
              cursorColumn={cursorPosition?.column ?? null}
            />
          </div>
        </IdeWorkspace>
      </RootLayout>

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
