import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';

import { IdeMonacoEditor } from '@boundary/editor/IdeMonacoEditor';
import { IdeToolDock } from '@boundary/layout/IdeToolDock';
import { TerminalFooterPanel } from '@boundary/terminal/TerminalFooterPanel';
import { IdeSessionProvider } from '@boundary/workspace/IdeSessionContext';
import { ToolDockProvider } from '@boundary/workspace/ToolDockContext';
import { useWorkspaceRoot } from '@boundary/workspace/WorkspaceContext';
import { WorkspaceFileTree } from '@boundary/workspace/WorkspaceFileTree';
import { loadPreferences } from '@infrastructure/preferences/preferencesBridge';

import styles from './PopoutView.module.css';

/**
 * Route component for /popout/:id. Each Tauri window has its own JS context,
 * so the pane registry from the main window isn't visible here. Instead we
 * render the requested pane directly, inside locally-mounted IDE providers.
 *
 * Until proper cross-window state sync lands, the popped-out window pulls
 * the most recent workspace from persisted preferences so it shows something
 * meaningful instead of a blank state.
 */
export function PopoutView() {
  const { id } = useParams<{ id: string }>();
  const paneId = id ?? '';

  return (
    <div className={styles.popoutShell}>
      <IdeSessionProvider>
        <ToolDockProvider>
          <PopoutPaneRenderer paneId={paneId} />
        </ToolDockProvider>
      </IdeSessionProvider>
    </div>
  );
}

function PopoutPaneRenderer({ paneId }: { paneId: string }) {
  const ctxWorkspaceRoot = useWorkspaceRoot();
  const [fallbackPath, setFallbackPath] = useState<string | null>(null);

  useEffect(() => {
    if (ctxWorkspaceRoot != null) {
      return;
    }
    void loadPreferences()
      .then((prefs) => {
        if (prefs.recentWorkspacePaths.length > 0) {
          setFallbackPath(prefs.recentWorkspacePaths[0] ?? null);
        }
      })
      .catch(() => undefined);
  }, [ctxWorkspaceRoot]);

  const workspaceRoot =
    ctxWorkspaceRoot ?? (fallbackPath != null ? { path: fallbackPath } : null);

  switch (paneId) {
    case 'fileTree':
      return <WorkspaceFileTree workspaceRoot={workspaceRoot} />;
    case 'editor':
      return <IdeMonacoEditor />;
    case 'toolDock':
      return <IdeToolDock />;
    case 'terminal':
      return <TerminalFooterPanel workspaceRoot={workspaceRoot?.path ?? null} />;
    default:
      return (
        <div className={styles.popoutPending} role="status">
          <p className={styles.popoutPendingTitle}>Unknown pane: {paneId || '(none)'}</p>
        </div>
      );
  }
}
