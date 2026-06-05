import { Fragment } from 'react';

import { workspaceBreadcrumb } from '@domain/workspace/paths';

import styles from './IdeStatusStrip.module.css';

export type IdeStatusStripProps = {
  activeFilePath: string | null;
  /** Workspace root — the breadcrumb is shown relative to it. */
  workspaceRoot: string | null;
  cursorLine: number | null;
  cursorColumn: number | null;
};

export function IdeStatusStrip({
  activeFilePath,
  workspaceRoot,
  cursorLine,
  cursorColumn,
}: IdeStatusStripProps) {
  // No active file → no status strip at all. Empty status fields were just
  // visual noise; the empty editor state already says nothing is open.
  if (activeFilePath == null || activeFilePath === '') {
    return null;
  }

  // Root-relative breadcrumb (Cursor-style) instead of the raw absolute path.
  const segments = workspaceBreadcrumb(activeFilePath, workspaceRoot);

  const cursorLabel =
    cursorLine != null && cursorColumn != null ? `Ln ${cursorLine}, Col ${cursorColumn}` : '';

  return (
    <div className={styles.statusStrip} aria-label="Editor status" data-testid="ide-status-strip">
      <span className={styles.fileCell} title={activeFilePath} aria-live="polite">
        {segments.map((seg, i) => (
          <Fragment key={i}>
            {i > 0 ? (
              <span className={styles.sep} aria-hidden>
                ›
              </span>
            ) : null}
            <span className={i === segments.length - 1 ? styles.crumbLeaf : styles.crumb}>
              {seg}
            </span>
          </Fragment>
        ))}
      </span>
      {cursorLabel !== '' ? <span className={styles.cursorCell}>{cursorLabel}</span> : null}
    </div>
  );
}
