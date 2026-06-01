import { BinaryToolPanel } from '@boundary/tools/BinaryToolPanel';
import { CodeEditorToolPanel } from '@boundary/tools/CodeEditorToolPanel';
import { DisassemblerToolPanel } from '@boundary/tools/DisassemblerToolPanel';
import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { TOOL_TAB_CATALOG } from '@domain/toolTabs/toolTabCatalog';
import type { ToolTabId } from '@domain/toolTabs/toolTabId';
import { disassemblerToolService } from '@infrastructure/disassembly/disassemblerToolService';

import styles from './IdeToolDock.module.css';

function renderPanel(id: ToolTabId) {
  if (id === 'binary') {
    return <BinaryToolPanel />;
  }
  if (id === 'codeEditor') {
    return <CodeEditorToolPanel />;
  }
  return <DisassemblerToolPanel disassembleFile={disassemblerToolService} />;
}

export function IdeToolDock() {
  const { activeToolTab, selectToolTab } = useToolDock();

  return (
    <aside className={styles.dockRoot} aria-label="Tool tabs">
      <div className={styles.rail} role="tablist" aria-orientation="vertical">
        {TOOL_TAB_CATALOG.map((tab) => {
          const active = activeToolTab === tab.id;
          return (
            <button
              key={tab.id}
              type="button"
              role="tab"
              aria-selected={active}
              className={`${styles.railBtn} ${active ? styles.railBtnActive : ''}`}
              title={tab.label}
              aria-label={tab.label}
              onClick={() => selectToolTab(tab.id)}
            >
              {tab.railLabel}
            </button>
          );
        })}
      </div>
      <div className={styles.panel}>
        {activeToolTab == null ? (
          <p className={styles.panelEmpty}>
            Select a tool on the rail (Binary, Code, Disasm) or use the Tools menu.
          </p>
        ) : (
          renderPanel(activeToolTab)
        )}
      </div>
    </aside>
  );
}
