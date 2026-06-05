import styles from './IdeStatusStrip.module.css';

export type IdeStatusStripProps = {
  activeFilePath: string | null;
  cursorLine: number | null;
  cursorColumn: number | null;
};

export function IdeStatusStrip({ activeFilePath, cursorLine, cursorColumn }: IdeStatusStripProps) {
  // No active file → no status strip at all. Empty status fields ("— (use
  // File → Open file)", "Ln —, Col —") were just visual noise; the empty
  // editor state already tells the user there's nothing open.
  if (activeFilePath == null || activeFilePath === '') {
    return null;
  }

  const cursorLabel =
    cursorLine != null && cursorColumn != null ? `Ln ${cursorLine}, Col ${cursorColumn}` : '';

  return (
    <div className={styles.statusStrip} aria-label="Editor status" data-testid="ide-status-strip">
      <span className={styles.fileCell} title={activeFilePath} aria-live="polite">
        {activeFilePath}
      </span>
      {cursorLabel !== '' ? <span className={styles.cursorCell}>{cursorLabel}</span> : null}
    </div>
  );
}
