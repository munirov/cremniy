import styles from './IdeStatusStrip.module.css';

export type IdeStatusStripProps = {
  activeFilePath: string | null;
  cursorLine: number | null;
  cursorColumn: number | null;
};

export function IdeStatusStrip({ activeFilePath, cursorLine, cursorColumn }: IdeStatusStripProps) {
  // No active file → no status strip. The path now lives in the top breadcrumb,
  // so the bottom strip only carries the cursor position.
  if (activeFilePath == null || activeFilePath === '') {
    return null;
  }

  const cursorLabel =
    cursorLine != null && cursorColumn != null ? `Ln ${cursorLine}, Col ${cursorColumn}` : '';

  if (cursorLabel === '') {
    return null;
  }

  return (
    <div className={styles.statusStrip} aria-label="Editor status" data-testid="ide-status-strip">
      <span className={styles.cursorCell}>{cursorLabel}</span>
    </div>
  );
}
