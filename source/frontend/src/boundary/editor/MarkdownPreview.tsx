import { useCallback, type MouseEvent } from 'react';

import { convertFileSrc } from '@tauri-apps/api/core';

import { MarkdownView } from '@cremniy/markdown-view';
import { parentDirectoryPath, resolveRelativePath } from '@domain/workspace/paths';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

import styles from './MarkdownPreview.module.css';

/** A URL with a scheme (http:, https:, data:, mailto:, asset:…), a
 *  protocol-relative `//`, or a `#anchor` — i.e. NOT a workspace-relative path. */
function isAbsoluteRef(url: string): boolean {
  return /^[a-z][a-z0-9+.-]*:/i.test(url) || url.startsWith('//') || url.startsWith('#');
}

/**
 * Rendered Markdown view of the active document, shown in the editor body when
 * the Preview toggle is on (see IdeDockview). Wraps the reusable
 * {@link MarkdownView} package (full CommonMark + GFM) in a scrollable reading
 * column, and — GitHub-style — resolves the document's RELATIVE references
 * against its own folder: images load from disk via the asset protocol, and a
 * link to another workspace file opens it in the editor.
 */
export function MarkdownPreview({ source }: { source: string }) {
  const { activeFilePath, openFileFromWorkspace } = useIdeSession();
  const baseDir = activeFilePath != null ? parentDirectoryPath(activeFilePath) : '';

  // Relative image src → a real file on disk, served through the Tauri asset
  // protocol so the webview can load it. Remote/absolute URLs pass through.
  const transformImageUrl = useCallback(
    (src: string): string =>
      isAbsoluteRef(src) || baseDir === '' ? src : convertFileSrc(resolveRelativePath(baseDir, src)),
    [baseDir],
  );

  // Relative link → open that workspace file in the editor instead of trying to
  // navigate the webview to a dead URL. External links keep their new-tab default.
  const onLinkClick = useCallback(
    (href: string, event: MouseEvent<HTMLAnchorElement>): void => {
      if (href === '' || isAbsoluteRef(href) || baseDir === '') {
        return;
      }
      event.preventDefault();
      void openFileFromWorkspace(resolveRelativePath(baseDir, href));
    },
    [baseDir, openFileFromWorkspace],
  );

  return (
    <div className={styles.root}>
      <div className={styles.prose}>
        <MarkdownView source={source} transformImageUrl={transformImageUrl} onLinkClick={onLinkClick} />
      </div>
    </div>
  );
}
