import { IdeBreadcrumb } from '@boundary/layout/IdeBreadcrumb';
import { Pane } from '@boundary/layout/Pane';
import { CodeEditorToolPanel } from '@boundary/tools/CodeEditorToolPanel';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';
import { useToolDock } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { fileNameFromPath } from '@domain/workspace/paths';
import { pluginToolTabs } from '@shared/plugins/registry';

import styles from './IdeToolDock.module.css';

/**
 * The tool pane — rendered between the center stack and the right-edge
 * ToolRail. Visible only when a tool is selected on the rail. Holds the
 * currently-active tool's panel inside a regular Pane wrapper so the
 * popout-to-window flow keeps working.
 */
export type IdeToolDockProps = {
  /** When the tool shares the editor slot via a split, this toggles the split
      off. Provided only in the split — it renders the toggle on this (right)
      pane so the control sits at the far edge, common to the split. */
  onToggleSplit?: () => void;
};

export function IdeToolDock({ onToggleSplit }: IdeToolDockProps = {}) {
  const { activeToolTab } = useToolDock();
  const { activeFilePath } = useIdeSession();
  const workspaceRoot = useWorkspaceRoot();
  if (activeToolTab == null) {
    return null;
  }
  // `codeEditor` is the editor-split mirror, not a rail tool — it stays in core.
  // Every other tool is a plugin contribution looked up by id from the registry.
  const isCodeEditor = activeToolTab === 'codeEditor';
  const tool = isCodeEditor ? null : pluginToolTabs().find((t) => t.id === activeToolTab);
  // The split's second pane mirrors the active file — label it with the file
  // name so it reads like a second editor, not a generic "Tool".
  const headerLabel = isCodeEditor
    ? fileNameFromPath(activeFilePath ?? '') || 'Editor'
    : tool?.label ?? 'Tool';
  const body = isCodeEditor ? <CodeEditorToolPanel /> : tool?.render() ?? null;
  return (
    <Pane id="toolDock" title={tool?.label ?? 'Tools'}>
      <div className={styles.toolStack}>
        <div className={styles.toolHeader}>
          <span className={styles.toolTab}>{headerLabel}</span>
          {onToggleSplit != null ? (
            <button
              type="button"
              className={`${styles.splitBtn} ${styles.splitBtnActive}`}
              onClick={onToggleSplit}
              title="Unsplit editor"
              aria-label="Unsplit editor"
            >
              <svg aria-hidden width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
                <rect x="3" y="4" width="18" height="16" rx="1.5" />
                <path d="M12 4v16" />
              </svg>
            </button>
          ) : null}
        </div>
        <IdeBreadcrumb filePath={activeFilePath} workspaceRoot={workspaceRoot?.path ?? null} />
        <div className={styles.toolBody}>{body}</div>
      </div>
    </Pane>
  );
}
