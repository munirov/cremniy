import { Fragment } from 'react';

import { workspaceBreadcrumb } from '@domain/workspace/paths';

import styles from './IdeBreadcrumb.module.css';

export type IdeBreadcrumbProps = {
  filePath: string | null;
  /** Workspace root — segments are shown relative to it (Cursor-style). */
  workspaceRoot: string | null;
};

/**
 * Path breadcrumb shown at the top of an editor/tool pane (under the tabs),
 * relative to the workspace root: `test-c › src › main.c`. Renders nothing
 * when there's no file.
 */
export function IdeBreadcrumb({ filePath, workspaceRoot }: IdeBreadcrumbProps) {
  if (filePath == null || filePath === '') {
    return null;
  }
  const segments = workspaceBreadcrumb(filePath, workspaceRoot);
  if (segments.length === 0) {
    return null;
  }
  return (
    <div className={styles.bar} aria-label="Breadcrumb" title={filePath}>
      {segments.map((seg, i) => (
        <Fragment key={i}>
          {i > 0 ? (
            <span className={styles.sep} aria-hidden>
              ›
            </span>
          ) : null}
          <span className={i === segments.length - 1 ? styles.leaf : styles.crumb}>{seg}</span>
        </Fragment>
      ))}
    </div>
  );
}
