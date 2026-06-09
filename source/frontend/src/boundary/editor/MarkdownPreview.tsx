import { MarkdownView } from '@cremniy/markdown-view';

import styles from './MarkdownPreview.module.css';

/**
 * Rendered Markdown view of the active document, shown in the editor body when
 * the Preview toggle is on (see IdeDockview). A thin wrapper around the reusable
 * {@link MarkdownView} package (full CommonMark + GFM) that adds a scrollable,
 * padded reading column on the dark editor background.
 */
export function MarkdownPreview({ source }: { source: string }) {
  return (
    <div className={styles.root}>
      <div className={styles.prose}>
        <MarkdownView source={source} />
      </div>
    </div>
  );
}
