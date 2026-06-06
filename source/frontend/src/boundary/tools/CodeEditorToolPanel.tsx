import Editor from '@monaco-editor/react';

import { monacoLanguageForPath } from '@domain/editor/editorLanguage';
import editorStyles from '@boundary/editor/IdeMonacoEditor.module.css';
import { IDE_MONACO_BASE_OPTIONS } from '@boundary/editor/ideMonacoSharedOptions';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

export function CodeEditorToolPanel() {
  const { documentText, activeFilePath } = useIdeSession();
  const language = monacoLanguageForPath(activeFilePath);

  return (
    <div className={editorStyles.wrapper} aria-label="Code editor tool">
      <Editor
        height="100%"
        language={language}
        theme="cremniy-dark"
        beforeMount={(monaco) => {
          // Active line as a fill, not vs-dark's outline (matches the main editor).
          monaco.editor.defineTheme('cremniy-dark', {
            base: 'vs-dark',
            inherit: true,
            rules: [],
            colors: {
              'editor.lineHighlightBackground': '#ffffff12',
              'editor.lineHighlightBorder': '#00000000',
            },
          });
        }}
        value={documentText}
        options={{
          ...IDE_MONACO_BASE_OPTIONS,
          readOnly: true,
          ariaLabel: 'Document mirror',
        }}
      />
    </div>
  );
}
