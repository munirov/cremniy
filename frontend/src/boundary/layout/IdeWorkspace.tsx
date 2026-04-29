import type { ReactNode } from 'react';

import type { WorkspaceRoot } from '@domain/workspace/types';

import styles from './IdeWorkspace.module.css';

export type IdeWorkspaceProps = {
  children?: ReactNode;
  workspaceRoot?: WorkspaceRoot | null;
  onCloseWorkspace?: () => void;
};

export function IdeWorkspace({
  children,
  workspaceRoot = null,
  onCloseWorkspace,
}: IdeWorkspaceProps) {
  const workspaceLabel = workspaceRoot?.path ?? '';

  return (
    <div className={styles.ideRoot}>
      <div className={styles.ideHorizontalSplit}>
        <aside aria-label="Files" className={styles.filesSidebar}>
          <p className={styles.workspaceStrip} title={workspaceLabel || undefined}>
            Workspace: {workspaceLabel ? workspaceLabel : '—'}
          </p>
          <div className={styles.sidebarMiddle}>
            <p className={styles.placeholder}>Files tree placeholder</p>
          </div>
          {onCloseWorkspace != null ? (
            <div className={styles.sidebarFooter}>
              <button type="button" className={styles.closeWorkspaceBtn} onClick={onCloseWorkspace}>
                Close workspace
              </button>
            </div>
          ) : null}
        </aside>
        <div className={styles.ideCenterColumn}>
          <div aria-label="Open tabs placeholder" className={styles.tabStrip} role="region">
            <p className={styles.placeholder}>Tab strip placeholder</p>
          </div>
          <section aria-label="Editor" className={styles.editorBody}>
            {children ?? <p className={styles.placeholder}>Editor body placeholder</p>}
          </section>
        </div>
      </div>
    </div>
  );
}
