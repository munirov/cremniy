import type { editor } from 'monaco-editor';
import Editor from '@monaco-editor/react';
import { useEffect, useRef, type MutableRefObject } from 'react';

import { monacoLanguageForPath } from '@domain/editor/editorLanguage';
import { useIdeSession } from '@boundary/workspace/IdeSessionContext';

import { IDE_MONACO_BASE_OPTIONS } from './ideMonacoSharedOptions';
import styles from './IdeMonacoEditor.module.css';

export type IdeEditorCommand =
  | {
      kind: 'findInEditor';
      nonce: number;
    };

export type IdeEditorCursorPosition = {
  line: number;
  column: number;
};

export type IdeMonacoEditorProps = {
  onCursorPositionChange?: (position: IdeEditorCursorPosition | null) => void;
  wordWrapEnabled?: boolean;
  command?: IdeEditorCommand | null;
};

function wireCursorReporting(
  editorInstance: editor.IStandaloneCodeEditor,
  onCursorPositionChangeRef: MutableRefObject<
    IdeMonacoEditorProps['onCursorPositionChange']
  >,
) {
  const emitIfFocused = () => {
    if (!editorInstance.hasTextFocus()) {
      return;
    }
    const pos = editorInstance.getPosition();
    if (pos != null) {
      onCursorPositionChangeRef.current?.({ line: pos.lineNumber, column: pos.column });
    }
  };

  const disposables = [
    editorInstance.onDidChangeCursorPosition(() => {
      emitIfFocused();
    }),
    editorInstance.onDidFocusEditorWidget(() => {
      emitIfFocused();
    }),
    editorInstance.onDidBlurEditorWidget(() => {
      onCursorPositionChangeRef.current?.(null);
    }),
  ];

  return () => {
    for (const d of disposables) {
      d.dispose();
    }
  };
}

export function IdeMonacoEditor({
  onCursorPositionChange,
  wordWrapEnabled = true,
  command = null,
}: IdeMonacoEditorProps) {
  const { documentText, setDocumentText, activeFilePath } = useIdeSession();
  const language = monacoLanguageForPath(activeFilePath);
  const editorRef = useRef<editor.IStandaloneCodeEditor | null>(null);
  const disposeCursorReportingRef = useRef<(() => void) | null>(null);
  const onCursorPositionChangeRef = useRef(onCursorPositionChange);
  onCursorPositionChangeRef.current = onCursorPositionChange;

  useEffect(
    () => () => {
      disposeCursorReportingRef.current?.();
      disposeCursorReportingRef.current = null;
      editorRef.current = null;
    },
    [],
  );

  useEffect(() => {
    editorRef.current?.updateOptions({ wordWrap: wordWrapEnabled ? 'on' : 'off' });
  }, [wordWrapEnabled]);

  useEffect(() => {
    if (command?.kind !== 'findInEditor') {
      return;
    }
    const editorInstance = editorRef.current;
    if (editorInstance == null) {
      return;
    }
    editorInstance.focus();
    void editorInstance.getAction('actions.find')?.run();
  }, [command]);

  return (
    <div className={styles.wrapper}>
      <Editor
        height="100%"
        language={language}
        path={activeFilePath ?? undefined}
        theme="vs"
        value={documentText}
        onChange={(value) => setDocumentText(value ?? '')}
        onMount={(editorInstance) => {
          editorRef.current = editorInstance;
          editorInstance.updateOptions({ wordWrap: wordWrapEnabled ? 'on' : 'off' });
          disposeCursorReportingRef.current?.();
          disposeCursorReportingRef.current = wireCursorReporting(
            editorInstance,
            onCursorPositionChangeRef,
          );
        }}
        options={{
          ...IDE_MONACO_BASE_OPTIONS,
          wordWrap: wordWrapEnabled ? 'on' : 'off',
          ariaLabel: 'Active document',
        }}
      />
    </div>
  );
}
