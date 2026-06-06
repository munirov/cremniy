import { IdeBreadcrumb } from '@boundary/layout/IdeBreadcrumb';
import { Pane } from '@boundary/layout/Pane';
import { BinaryToolPanel } from '@boundary/tools/BinaryToolPanel';
import { CodeEditorToolPanel } from '@boundary/tools/CodeEditorToolPanel';
import { DisassemblerToolPanel } from '@boundary/tools/DisassemblerToolPanel';
import { StringsToolPanel } from '@boundary/tools/StringsToolPanel';
import { FunctionListToolPanel } from '@boundary/tools/FunctionListToolPanel';
import { MemoryMapToolPanel } from '@boundary/tools/MemoryMapToolPanel';
import { PatchesToolPanel } from '@boundary/tools/PatchesToolPanel';
import { ResourcesToolPanel } from '@boundary/tools/ResourcesToolPanel';
import { SymbolTableToolPanel } from '@boundary/tools/SymbolTableToolPanel';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { TOOL_TAB_CATALOG } from '@domain/toolTabs/toolTabCatalog';
import type { ToolTabId } from '@domain/toolTabs/toolTabId';
import { disassemblerToolService } from '@infrastructure/disassembly/disassemblerToolService';

import styles from './IdeToolDock.module.css';

function renderPanel(id: ToolTabId) {
  if (id === 'binary') return <BinaryToolPanel />;
  if (id === 'codeEditor') return <CodeEditorToolPanel />;
  if (id === 'strings') return <StringsToolPanel />;
  if (id === 'symbols') return <SymbolTableToolPanel />;
  if (id === 'memoryMap') return <MemoryMapToolPanel />;
  if (id === 'functions') return <FunctionListToolPanel />;
  if (id === 'patches') return <PatchesToolPanel />;
  if (id === 'resources') return <ResourcesToolPanel />;
  return <DisassemblerToolPanel disassembleFile={disassemblerToolService} />;
}

/**
 * The tool pane — rendered between the center stack and the right-edge
 * ToolRail. Visible only when a tool is selected on the rail. Holds the
 * currently-active tool's panel inside a regular Pane wrapper so the
 * popout-to-window flow keeps working.
 */
export function IdeToolDock() {
  const { activeToolTab } = useToolDock();
  const { activeFilePath } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  if (activeToolTab == null) {
    return null;
  }
  const entry = TOOL_TAB_CATALOG.find((t) => t.id === activeToolTab);
  return (
    <Pane id="toolDock" title={entry?.label ?? 'Tools'}>
      <div className={styles.toolStack}>
        <div className={styles.toolHeader}>
          <span className={styles.toolTab}>{entry?.label ?? 'Tool'}</span>
        </div>
        <IdeBreadcrumb filePath={activeFilePath} workspaceRoot={workspaceRoot?.path ?? null} />
        <div className={styles.toolBody}>{renderPanel(activeToolTab)}</div>
      </div>
    </Pane>
  );
}
