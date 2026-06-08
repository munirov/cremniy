import { useCallback, useEffect, useMemo, useRef, useState, type ReactNode } from 'react';

import type { IdeEditorCommand, IdeEditorCursorPosition } from '@boundary/editor/IdeMonacoEditor';
import { IdeMonacoEditor } from '@boundary/editor/IdeMonacoEditor';
import { BinaryFilePlaceholder } from '@boundary/editor/BinaryFilePlaceholder';
import { ImageTab } from '@boundary/editor/ImageTab';
import { IdeBreadcrumb } from '@boundary/layout/IdeBreadcrumb';
import { IdeEditorTabStrip } from '@boundary/layout/IdeEditorTabStrip';
import { resolveCenterPanel } from '@boundary/layout/centerPanels';
import { pluginToolTabs } from '@shared/plugins/registry';
import { useRegistryVersion } from '@shared/plugins/useRegistry';
import { IdeStatusStrip } from '@boundary/layout/IdeStatusStrip';
import { IdeToolDock } from '@boundary/layout/IdeToolDock';
import { Pane } from '@boundary/layout/Pane';
import { SplitContainer } from '@boundary/layout/SplitContainer';
import { ToolRail } from '@boundary/layout/ToolRail';
import { TerminalFooterPanel } from '@boundary/terminal/TerminalFooterPanel';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { SidePanel } from '@boundary/workspace/SidePanel';
import type { WorkspaceRoot } from '@domain/workspace/types';

import styles from './IdeDockview.module.css';

type BuiltinPaneId = 'fileTree' | 'editor' | 'toolDock' | 'terminal';

const BUILTIN_PANES: Array<{ id: BuiltinPaneId; label: string }> = [
  { id: 'fileTree', label: 'Files' },
  { id: 'editor', label: 'Editor' },
  { id: 'toolDock', label: 'Tools' },
  { id: 'terminal', label: 'Terminal' },
];

export type IdeLayoutSizes = {
  /** Horizontal outer split: [fileTree, center, toolPane?]. */
  outer: number[];
  /** Vertical split inside the center column: [editor, terminal]. */
  center: number[];
};

const DEFAULT_LAYOUT: IdeLayoutSizes = {
  // File tree ~20% of the width (1.5 / (1.5 + 6) with the tool dock hidden).
  outer: [1.5, 6, 3],
  center: [4, 1],
};

/** Height of the terminal when minimised — just its tab bar (see TerminalFooterPanel.module.css). */
const TERMINAL_STRIP_PX = 30;

export type IdeDockviewProps = {
  workspaceRoot: WorkspaceRoot | null;
  editorCommand: IdeEditorCommand | null;
  wordWrapEnabled: boolean;
  editorInsertSpaces?: boolean;
  editorTabWidth?: number;
  onCursorPositionChange: (position: IdeEditorCursorPosition | null) => void;
  cursorPosition: IdeEditorCursorPosition | null;
  editorFontSize?: number;
  onEditorFontSizeChange?: (size: number) => void;
  /** Serialized layout to restore on mount (`null` → default layout). */
  initialLayout?: unknown | null;
  /** Fired (debounced inside us) when the user resizes a split. */
  onLayoutChange?: (layout: IdeLayoutSizes) => void;
  /** Show / hide the terminal pane via the View menu. */
  terminalVisible?: boolean;
  /** Incrementing counter from Terminal → New Terminal; spawns a tab in the panel. */
  newTerminalSignal?: number;
  /** Collapse the terminal panel (its own hide / close buttons call this). */
  onHideTerminal?: () => void;
  /** Called when the user toggles a pane's visibility from the dock context menu. */
  onTogglePane?: (id: BuiltinPaneId) => void;
};

/**
 * IDE shell layout.
 *
 *   ┌─────────┬───────────────┬──────────┬──┐
 *   │         │   Editor      │          │  │
 *   │  Files  ├───────────────┤  Tool    │R │
 *   │  Tree   │   Terminal    │  Pane    │ail
 *   │         │               │ (opt.)   │  │
 *   └─────────┴───────────────┴──────────┴──┘
 *
 *   - FileTree spans the full height (terminal sits ONLY under the editor)
 *   - Editor + Terminal are stacked in the center column via a vertical split
 *   - ToolRail is a fixed-width strip on the far right, always visible —
 *     clicking a tool opens it in ToolPane between center and rail; clicking
 *     the active tool again closes the pane
 *   - The dividers between FileTree / Center / ToolPane are draggable
 */
export function IdeDockview({
  workspaceRoot,
  editorCommand,
  wordWrapEnabled,
  editorInsertSpaces = false,
  editorTabWidth = 4,
  onCursorPositionChange,
  editorFontSize = 14,
  onEditorFontSizeChange,
  cursorPosition,
  initialLayout = null,
  onLayoutChange,
  terminalVisible = true,
  newTerminalSignal = 0,
  onHideTerminal,
  onTogglePane,
  paneVisibility: paneVisibilityProp,
}: IdeDockviewProps & { paneVisibility?: Partial<Record<BuiltinPaneId, boolean>> }) {
  useRegistryVersion(); // re-render the center-panel body when a plugin is toggled
  const [dockMenu, setDockMenu] = useState<{ x: number; y: number } | null>(null);
  // Terminal minimised to its tab strip (sessions stay alive). Local layout
  // state — not persisted.
  const [terminalCollapsed, setTerminalCollapsed] = useState(false);

  useEffect(() => {
    if (dockMenu == null) {
      return;
    }
    const onDown = (ev: PointerEvent) => {
      const target = ev.target as HTMLElement | null;
      if (target == null || !target.closest('[data-dock-menu]')) {
        setDockMenu(null);
      }
    };
    const onKey = (ev: KeyboardEvent) => {
      if (ev.key === 'Escape') {
        setDockMenu(null);
      }
    };
    window.addEventListener('pointerdown', onDown);
    window.addEventListener('keydown', onKey);
    return () => {
      window.removeEventListener('pointerdown', onDown);
      window.removeEventListener('keydown', onKey);
    };
  }, [dockMenu]);

  const handleHostContextMenu = useCallback((e: React.MouseEvent) => {
    if (e.target !== e.currentTarget) {
      return;
    }
    e.preventDefault();
    setDockMenu({ x: e.clientX, y: e.clientY });
  }, []);

  const initial = useMemo(() => parseLayout(initialLayout), [initialLayout]);

  const onLayoutChangeRef = useRef(onLayoutChange);
  onLayoutChangeRef.current = onLayoutChange;

  const layoutRef = useRef<IdeLayoutSizes>(initial);
  layoutRef.current = initial;

  const saveTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const scheduleSave = useCallback(() => {
    if (saveTimer.current != null) {
      clearTimeout(saveTimer.current);
    }
    saveTimer.current = setTimeout(() => {
      onLayoutChangeRef.current?.(layoutRef.current);
    }, 250);
  }, []);

  useEffect(
    () => () => {
      if (saveTimer.current != null) {
        clearTimeout(saveTimer.current);
      }
    },
    [],
  );

  const handleOuterSizes = useCallback(
    (sizes: number[]) => {
      layoutRef.current = { ...layoutRef.current, outer: sizes };
      scheduleSave();
    },
    [scheduleSave],
  );
  const handleCenterSizes = useCallback(
    (sizes: number[]) => {
      layoutRef.current = { ...layoutRef.current, center: sizes };
      scheduleSave();
    },
    [scheduleSave],
  );

  const ide = useIdeSession();
  const { activeToolTab, setActiveToolTab } = useToolDock();

  // Split the editor slot: show the editor AND the active tool side by side
  // instead of the tool replacing the editor. Toggling it on with no tool open
  // defaults the second pane to a code editor (≈ two editors). Local layout
  // state, not persisted.
  const [editorSplit, setEditorSplit] = useState(false);
  const toggleEditorSplit = useCallback(() => {
    const next = !editorSplit;
    setEditorSplit(next);
    if (next) {
      if (activeToolTab == null) {
        setActiveToolTab('codeEditor');
      }
    } else {
      // Un-split → back to a single editor. Clear the tool that was sharing the
      // slot, otherwise it stays full-screen and the editor looks "broken".
      setActiveToolTab(null);
    }
  }, [editorSplit, activeToolTab, setActiveToolTab]);

  const paneVisibility: Record<BuiltinPaneId, boolean> = {
    fileTree: paneVisibilityProp?.fileTree ?? true,
    editor: paneVisibilityProp?.editor ?? true,
    // The tool pane is driven by the rail's active selection — the View-menu
    // checkbox just clears the active tool when the user unchecks it.
    toolDock: activeToolTab != null,
    terminal: paneVisibilityProp?.terminal ?? terminalVisible,
  };

  // Trim Windows extended-length prefix (`\\?\`) from the displayed root so
  // the Pane header reads "C:\path\..." instead of "\\?\C:\...".
  const prettyRoot =
    workspaceRoot?.path?.replace(/^\\\\\?\\/, '').replace(/^\/\/\?\//, '') ?? '';
  const fileTreeNode = paneVisibility.fileTree ? (
    <Pane id="fileTree" title={prettyRoot !== '' ? `Files — ${prettyRoot}` : 'Files'}>
      <SidePanel workspaceRoot={workspaceRoot} />
    </Pane>
  ) : null;

  // The editor pane (Monaco + tabs + status). Always built when the editor is
  // visible; how it shares the slot with a tool is decided below.
  const editorPaneNode = paneVisibility.editor ? (
    <Pane
      id="editor"
      title={ide.activeFilePath ?? 'Editor'}
      popoutRender={() => (
        <div className={styles.popoutStub}>
          Editor in a separate window is a stub.
          <br />
          Session sync between windows is coming next.
        </div>
      )}
    >
      <div className={styles.editorStack}>
        {ide.openFilePaths.length > 0 || ide.openPanels.length > 0 ? (
          <div className={styles.tabStrip} role="region" aria-label="Document tabs">
            <IdeEditorTabStrip />
            {/* While split, the toggle lives on the right pane (far edge) so it's
                not stuck to the left window — see IdeToolDock's onToggleSplit. */}
            {!editorSplit ? (
              <button
                type="button"
                className={styles.splitBtn}
                onClick={toggleEditorSplit}
                title="Split editor (open a tool / second editor beside)"
                aria-label="Split editor"
              >
                <svg aria-hidden width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                  <rect x="3" y="4" width="18" height="16" rx="1.5" />
                  <path d="M12 4v16" />
                </svg>
              </button>
            ) : null}
          </div>
        ) : null}
        {ide.activePanel != null ? (
          <div className={styles.editorBody}>{resolveCenterPanel(ide.activePanel)?.render() ?? null}</div>
        ) : ide.activeFileIsBinary && isImagePath(ide.activeFilePath) ? (
          <>
            <IdeBreadcrumb
              filePath={ide.activeFilePath}
              workspaceRoot={workspaceRoot?.path ?? null}
            />
            <div className={styles.editorBody}>
              <ImageTab filePath={ide.activeFilePath} />
            </div>
          </>
        ) : ide.activeFileIsBinary ? (
          <>
            <IdeBreadcrumb
              filePath={ide.activeFilePath}
              workspaceRoot={workspaceRoot?.path ?? null}
            />
            <div className={styles.editorBody}>
              <BinaryFilePlaceholder filePath={ide.activeFilePath} />
            </div>
          </>
        ) : (
          <>
            <IdeBreadcrumb
              filePath={ide.activeFilePath}
              workspaceRoot={workspaceRoot?.path ?? null}
            />
            <div className={styles.editorBody}>
              <IdeMonacoEditor
                onCursorPositionChange={onCursorPositionChange}
                wordWrapEnabled={wordWrapEnabled}
                insertSpaces={editorInsertSpaces}
                tabWidth={editorTabWidth}
                fontSize={editorFontSize}
                onFontSizeChange={onEditorFontSizeChange}
                command={editorCommand}
              />
            </div>
            <IdeStatusStrip
              activeFilePath={ide.activeFilePath}
              cursorLine={cursorPosition?.line ?? null}
              cursorColumn={cursorPosition?.column ?? null}
            />
          </>
        )}
      </div>
    </Pane>
  ) : null;

  const toolNode = activeToolTab != null ? <IdeToolDock /> : null;

  // Editor slot: split → editor + tool side by side; otherwise the tool replaces
  // the editor (legacy) or the editor shows alone.
  const editorNode: ReactNode = (() => {
    if (editorSplit && editorPaneNode != null && toolNode != null) {
      return (
        <SplitContainer direction="horizontal" defaultSizes={[1, 1]}>
          <div key="editor-pane" style={{ width: '100%', height: '100%' }}>
            {editorPaneNode}
          </div>
          <div key="tool-pane" style={{ width: '100%', height: '100%' }}>
            {/* The split toggle rides on this (right) pane so it sits at the far
                edge, common to the split, not buried in the left window. */}
            <IdeToolDock onToggleSplit={toggleEditorSplit} />
          </div>
        </SplitContainer>
      );
    }
    if (toolNode != null && !editorSplit) {
      return toolNode;
    }
    return editorPaneNode ?? toolNode;
  })();

  const terminalNode = paneVisibility.terminal ? (
    <Pane id="terminal" title="Terminal">
      <TerminalFooterPanel
        workspaceRoot={workspaceRoot?.path ?? null}
        newTerminalSignal={newTerminalSignal}
        collapsed={terminalCollapsed}
        onCollapse={() => setTerminalCollapsed(true)}
        onExpand={() => setTerminalCollapsed(false)}
        onClose={() => {
          setTerminalCollapsed(false);
          onHideTerminal?.();
        }}
      />
    </Pane>
  ) : null;

  // Right-column tool pane removed — tools now open in the editor slot.
  const toolPaneNode: ReactNode = null;

  // Center column = vertical split [Editor, Terminal]. If terminal is hidden,
  // editor takes the whole column.
  const centerNode: ReactNode = (() => {
    if (editorNode == null && terminalNode == null) {
      return (
        <div className={styles.dockEmpty} role="status">
          Editor and terminal are hidden. Right-click here to bring them back.
        </div>
      );
    }
    if (editorNode != null && terminalNode != null) {
      return (
        <SplitContainer
          direction="vertical"
          defaultSizes={initial.center}
          onSizesChange={handleCenterSizes}
          collapsed={[false, terminalCollapsed]}
        >
          <div key="editor" style={{ width: '100%', height: '100%' }}>
            {editorNode}
          </div>
          <div
            key="terminal"
            style={{ width: '100%', height: terminalCollapsed ? TERMINAL_STRIP_PX : '100%' }}
          >
            {terminalNode}
          </div>
        </SplitContainer>
      );
    }
    return editorNode ?? terminalNode;
  })();

  // Outer horizontal split: [fileTree?, center, toolPane?]. Empty slots are
  // skipped, so a hidden FileTree just gives the center more room.
  type OuterEntry = { key: 'fileTree' | 'center' | 'toolDock'; node: ReactNode };
  const outerCandidates: Array<OuterEntry | null> = [
    fileTreeNode != null ? { key: 'fileTree', node: fileTreeNode } : null,
    { key: 'center', node: centerNode },
    toolPaneNode != null ? { key: 'toolDock', node: toolPaneNode } : null,
  ];
  const outerEntries = outerCandidates.filter((x): x is OuterEntry => x != null);

  const outerSizes = outerEntries.map((e) => {
    if (e.key === 'fileTree') return initial.outer[0] ?? 2;
    if (e.key === 'center') return initial.outer[1] ?? 6;
    return initial.outer[2] ?? 3;
  });

  return (
    <div className={styles.dockHost} onContextMenu={handleHostContextMenu}>
      <div style={{ flex: 1, minWidth: 0, display: 'flex' }}>
        {outerEntries.length === 1 ? (
          outerEntries[0]!.node
        ) : (
          <SplitContainer
            direction="horizontal"
            defaultSizes={outerSizes}
            onSizesChange={handleOuterSizes}
          >
            {outerEntries.map((e) => (
              <div key={e.key} style={{ width: '100%', height: '100%' }}>
                {e.node}
              </div>
            ))}
          </SplitContainer>
        )}
      </div>
      {/* The rail is a launcher for the byte tools — hide it entirely when no
          tool plugin contributes icons (e.g. Binary Tools disabled), so an empty
          right strip doesn't linger. */}
      {pluginToolTabs().length > 0 ? <ToolRail /> : null}

      {dockMenu != null ? (
        <ul
          data-dock-menu
          className={styles.dockCtxMenu}
          style={{ left: dockMenu.x, top: dockMenu.y }}
          role="menu"
        >
          <li role="presentation" className={styles.dockCtxHeader}>
            View
          </li>
          {BUILTIN_PANES.map((p) => {
            const visible = paneVisibility[p.id];
            return (
              <li role="none" key={p.id}>
                <button
                  type="button"
                  role="menuitemcheckbox"
                  aria-checked={visible}
                  className={styles.dockCtxItem}
                  onClick={() => {
                    onTogglePane?.(p.id);
                    setDockMenu(null);
                  }}
                >
                  <span className={styles.dockCtxCheck}>{visible ? '✓' : ' '}</span>
                  <span>{p.label}</span>
                </button>
              </li>
            );
          })}
        </ul>
      ) : null}
    </div>
  );
}

/** Raster image extensions we render in an ImageTab instead of the byte placeholder. */
const IMAGE_EXTENSIONS: ReadonlySet<string> = new Set([
  'png',
  'jpg',
  'jpeg',
  'gif',
  'webp',
  'bmp',
  'ico',
]);

/** True when the path's (lowercased) extension is a raster image we can preview. */
function isImagePath(path: string | null): boolean {
  if (path == null) {
    return false;
  }
  const dot = path.lastIndexOf('.');
  if (dot < 0) {
    return false;
  }
  return IMAGE_EXTENSIONS.has(path.slice(dot + 1).toLowerCase());
}

function parseLayout(raw: unknown): IdeLayoutSizes {
  if (raw == null || typeof raw !== 'object') {
    return DEFAULT_LAYOUT;
  }
  const o = raw as Record<string, unknown>;
  const outer = sanitizeSizes(o.outer, DEFAULT_LAYOUT.outer.length, DEFAULT_LAYOUT.outer);
  // Tolerate the legacy { center: ... } *and* the older { topRow: ... } shape.
  const centerRaw = o.center ?? o.topRow;
  const center = sanitizeSizes(centerRaw, DEFAULT_LAYOUT.center.length, DEFAULT_LAYOUT.center);
  return { outer, center };
}

function sanitizeSizes(value: unknown, expectedLen: number, fallback: number[]): number[] {
  if (!Array.isArray(value) || value.length !== expectedLen) {
    return fallback.slice();
  }
  const cleaned = value.map((v, i) =>
    typeof v === 'number' && Number.isFinite(v) && v > 0 ? v : fallback[i] ?? 1,
  );
  return cleaned;
}
