import { IdeBreadcrumb } from '@boundary/layout/IdeBreadcrumb';
import { Pane } from '@boundary/layout/Pane';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { pluginToolTabs } from '@shared/plugins/registry';
import { useRegistryVersion } from '@shared/plugins/useRegistry';

import styles from './IdeToolDock.module.css';

/**
 * The tool pane — rendered in the editor slot when a rail tool is selected,
 * replacing the editor. Holds the active tool's panel inside a regular Pane
 * wrapper so the popout-to-window flow keeps working. Every tool is a plugin
 * contribution looked up by id from the registry.
 */
export function IdeToolDock() {
  const { activeToolTab } = useToolDock();
  const { activeFilePath } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  useRegistryVersion(); // re-render when a tool plugin is enabled/disabled
  if (activeToolTab == null) {
    return null;
  }
  const tool = pluginToolTabs().find((t) => t.id === activeToolTab);
  // The active tool's plugin was disabled — close the dock instead of an empty pane.
  if (tool == null) {
    return null;
  }
  return (
    <Pane id="toolDock" title={tool.label}>
      <div className={styles.toolStack}>
        <div className={styles.toolHeader}>
          <span className={styles.toolTab}>{tool.label}</span>
        </div>
        <IdeBreadcrumb filePath={activeFilePath} workspaceRoot={workspaceRoot?.path ?? null} />
        <div className={styles.toolBody}>{tool.render()}</div>
      </div>
    </Pane>
  );
}
