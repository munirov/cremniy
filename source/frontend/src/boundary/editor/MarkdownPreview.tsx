import { Markdown } from '@boundary/extensions/Markdown';

import styles from './MarkdownPreview.module.css';

/**
 * Rendered Markdown view of the active document, shown in the editor body when
 * the Preview toggle is on (see IdeDockview). A thin wrapper around the shared
 * {@link Markdown} renderer that adds a scrollable, padded reading column on the
 * dark editor background — the renderer itself owns all prose styling.
 */
export function MarkdownPreview({ source }: { source: string }) {
  return (
    <div className={styles.root}>
      <div className={styles.prose}>
        <Markdown source={source} />
      </div>
    </div>
  );
}
