import { useEffect, useState, type MouseEvent } from 'react';

import type { IdeEditorCommand, IdeEditorCursorPosition } from '@boundary/editor/IdeMonacoEditor';
import { IdeMonacoEditor } from '@boundary/editor/IdeMonacoEditor';
import { BinaryFilePlaceholder } from '@boundary/editor/BinaryFilePlaceholder';
import { ImageTab } from '@boundary/editor/ImageTab';
import { MarkdownPreview } from '@boundary/editor/MarkdownPreview';
import { IdeBreadcrumb } from '@boundary/layout/IdeBreadcrumb';
import { IdeEditorTabStrip } from '@boundary/layout/IdeEditorTabStrip';
import { resolveCenterPanel } from '@boundary/layout/centerPanels';
import { IdeStatusStrip } from '@boundary/layout/IdeStatusStrip';
import { useIdeSession, type EditorGroupView } from '@boundary/workspace/IdeSessionContext';
import type { WorkspaceRoot } from '@domain/workspace/types';

import styles from './GroupEditorPane.module.css';

export type GroupEditorPaneProps = {
  /** The group this pane renders (its tabs, active file, preview, panel). */
  group: EditorGroupView;
  editorCommand: IdeEditorCommand | null;
  wordWrapEnabled: boolean;
  editorInsertSpaces?: boolean;
  editorTabWidth?: number;
  editorFontSize?: number;
  onEditorFontSizeChange?: (size: number) => void;
  onCursorPositionChange: (position: IdeEditorCursorPosition | null) => void;
  cursorPosition: IdeEditorCursorPosition | null;
  workspaceRoot: WorkspaceRoot | null;
};

/**
 * The body of a single editor group: tab strip + (markdown) breadcrumb +
 * editor / preview / binary / image / center-panel branch + status strip. One
 * pane per group; the buffers stay global (read via `getBuffer`/`writeBuffer`).
 *
 * Clicking anywhere in the pane focuses its group. The active group's pane gets
 * a thin inset focus ring (only when more than one group exists, so the single
 * default group renders pixel-identically to the pre-groups editor pane).
 */
export function GroupEditorPane({
  group,
  editorCommand,
  wordWrapEnabled,
  editorInsertSpaces = false,
  editorTabWidth = 4,
  editorFontSize = 14,
  onEditorFontSizeChange,
  onCursorPositionChange,
  cursorPosition,
  workspaceRoot,
}: GroupEditorPaneProps) {
  const ide = useIdeSession();
  const { getBuffer, writeBuffer, isBinaryPath, focusEditorGroup, splitActiveFile, revealTarget } = ide;

  const activeFilePath = group.activeFilePath;
  const activePanel = group.activePanel;
  const activeIsBinary = activeFilePath != null && activeFilePath !== '' && isBinaryPath(activeFilePath);

  // Markdown source/preview toggle — per group. Reset to source whenever this
  // group's active file changes so a freshly opened file lands in the editor,
  // not someone else's stale preview.
  const isMarkdown = isMarkdownPath(activeFilePath);
  const [mdPreview, setMdPreview] = useState(false);
  useEffect(() => {
    setMdPreview(false);
  }, [activeFilePath]);
  const showMarkdownPreview = isMarkdown && mdPreview && activePanel == null && !activeIsBinary;

  // Only ring the focused pane when there's more than one group — a lone group
  // stays borderless (pixel-identical to the pre-groups shell).
  const focused = group.id === ide.activeGroupId && ide.editorGroups.length > 1;

  // Editor binding. A real file binds to its global buffer. With NO active file
  // (the scratch buffer), the active group binds to the global documentText
  // mirror + setDocumentText — preserving the pre-groups scratch behavior (type
  // before opening a file, then Save-As). A non-active group with no file shows
  // an empty editor (no scratch sharing across groups).
  const hasFile = activeFilePath != null && activeFilePath !== '';
  const isActiveGroup = group.id === ide.activeGroupId;
  const editorValue = hasFile
    ? getBuffer(activeFilePath!)
    : isActiveGroup
      ? ide.documentText
      : '';
  const onEditorChange = hasFile
    ? (t: string) => writeBuffer(activeFilePath!, t)
    : isActiveGroup
      ? ide.setDocumentText
      : () => undefined;

  const onPaneMouseDown = (_e: MouseEvent<HTMLDivElement>): void => {
    focusEditorGroup(group.id);
  };

  const hasTabs = group.openTabs.length > 0 || group.openPanels.length > 0;

  return (
    <div
      className={`${styles.editorStack}${focused ? ` ${styles.editorStackFocused}` : ''}`}
      onMouseDown={onPaneMouseDown}
    >
      {hasTabs ? (
        <div className={styles.tabStrip} role="region" aria-label="Document tabs">
          <IdeEditorTabStrip groupId={group.id} />
          {/* Markdown source/preview toggle — only for .md/.markdown in the
              source-editor branch, pinned top-right like VS Code. */}
          {isMarkdown && activePanel == null && !activeIsBinary ? (
            <div className={styles.mdToggle} role="group" aria-label="Markdown view">
              <button
                type="button"
                className={`${styles.mdToggleBtn} ${!mdPreview ? styles.mdToggleBtnActive : ''}`}
                onClick={() => setMdPreview(false)}
                aria-pressed={!mdPreview}
                title="Edit Markdown source"
              >
                Markdown
              </button>
              <button
                type="button"
                className={`${styles.mdToggleBtn} ${mdPreview ? styles.mdToggleBtnActive : ''}`}
                onClick={() => setMdPreview(true)}
                aria-pressed={mdPreview}
                title="Preview rendered Markdown"
              >
                Preview
              </button>
            </div>
          ) : null}
          {/* Split right: open the active file in a new group beside this one
              (VS Code "Split Editor Right"). Both views share the buffer. The
              pane's onMouseDown already focuses this group, so splitting acts on
              it; focusing again here is belt-and-suspenders for event ordering. */}
          <button
            type="button"
            className={styles.splitBtn}
            onClick={() => {
              focusEditorGroup(group.id);
              splitActiveFile('right');
            }}
            title="Split editor right"
            aria-label="Split editor right"
          >
            <svg aria-hidden width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
              <rect x="3" y="4" width="18" height="16" rx="1.5" />
              <path d="M12 4v16" />
            </svg>
          </button>
        </div>
      ) : null}
      {activePanel != null ? (
        <div className={styles.editorBody}>{resolveCenterPanel(activePanel)?.render() ?? null}</div>
      ) : activeIsBinary && isImagePath(activeFilePath) ? (
        <>
          <IdeBreadcrumb filePath={activeFilePath} workspaceRoot={workspaceRoot?.path ?? null} />
          <div className={styles.editorBody}>
            <ImageTab filePath={activeFilePath} />
          </div>
        </>
      ) : activeIsBinary ? (
        <>
          <IdeBreadcrumb filePath={activeFilePath} workspaceRoot={workspaceRoot?.path ?? null} />
          <div className={styles.editorBody}>
            <BinaryFilePlaceholder filePath={activeFilePath} />
          </div>
        </>
      ) : showMarkdownPreview ? (
        <>
          <IdeBreadcrumb filePath={activeFilePath} workspaceRoot={workspaceRoot?.path ?? null} />
          <div className={styles.editorBody}>
            <MarkdownPreview source={getBuffer(activeFilePath ?? '')} />
          </div>
        </>
      ) : (
        <>
          <IdeBreadcrumb filePath={activeFilePath} workspaceRoot={workspaceRoot?.path ?? null} />
          <div className={styles.editorBody}>
            <IdeMonacoEditor
              value={editorValue}
              onChange={onEditorChange}
              filePath={activeFilePath}
              revealTarget={revealTarget?.path === activeFilePath ? revealTarget : null}
              onCursorPositionChange={onCursorPositionChange}
              wordWrapEnabled={wordWrapEnabled}
              insertSpaces={editorInsertSpaces}
              tabWidth={editorTabWidth}
              fontSize={editorFontSize}
              onFontSizeChange={onEditorFontSizeChange}
              command={editorCommand}
            />
          </div>
          <IdeStatusStrip
            activeFilePath={activeFilePath}
            cursorLine={cursorPosition?.line ?? null}
            cursorColumn={cursorPosition?.column ?? null}
          />
        </>
      )}
    </div>
  );
}

/** Raster image extensions we render in an ImageTab instead of the byte placeholder. */
const IMAGE_EXTENSIONS: ReadonlySet<string> = new Set([
  'png',
  'jpg',
  'jpeg',
  'gif',
  'webp',
  'bmp',
  'ico',
]);

/** True when the path's (lowercased) extension is a raster image we can preview. */
function isImagePath(path: string | null): boolean {
  if (path == null) {
    return false;
  }
  const dot = path.lastIndexOf('.');
  if (dot < 0) {
    return false;
  }
  return IMAGE_EXTENSIONS.has(path.slice(dot + 1).toLowerCase());
}

/** True when the path's (lowercased) extension is `md` or `markdown`. */
function isMarkdownPath(path: string | null): boolean {
  if (path == null) {
    return false;
  }
  const dot = path.lastIndexOf('.');
  if (dot < 0) {
    return false;
  }
  const ext = path.slice(dot + 1).toLowerCase();
  return ext === 'md' || ext === 'markdown';
}
