import styles from './IdeStatusStrip.module.css';

export type IdeStatusStripProps = {
  activeFilePath: string | null;
  cursorLine: number | null;
  cursorColumn: number | null;
};

export function IdeStatusStrip({ activeFilePath, cursorLine, cursorColumn }: IdeStatusStripProps) {
  const fileLabel =
    activeFilePath != null && activeFilePath !== ''
      ? activeFilePath
      : '— (use File → Open file)';

  const cursorLabel =
    cursorLine != null && cursorColumn != null
      ? `Ln ${cursorLine}, Col ${cursorColumn}`
      : 'Ln —, Col —';

  return (
    <div className={styles.statusStrip} aria-label="Editor status" data-testid="ide-status-strip">
      <span
        className={styles.fileCell}
        title={activeFilePath ?? undefined}
        aria-live="polite"
      >
        {fileLabel}
      </span>
      <span className={styles.metaCell}>Encoding: —</span>
      <span className={styles.cursorCell}>{cursorLabel}</span>
    </div>
  );
}
