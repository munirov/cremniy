import { render } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { editor } from 'monaco-editor';

import { IdeMonacoEditor, type IdeEditorCommand } from './IdeMonacoEditor';

const ideSessionMocks = vi.hoisted(() => ({
  documentText: 'body',
  setDocumentText: vi.fn(),
}));

const monacoMocks = vi.hoisted(() => ({
  editorInstance: {
    updateOptions: vi.fn(),
    focus: vi.fn(),
    getAction: vi.fn(),
    hasTextFocus: vi.fn(),
    getPosition: vi.fn(),
    onDidChangeCursorPosition: vi.fn(),
    onDidFocusEditorWidget: vi.fn(),
    onDidBlurEditorWidget: vi.fn(),
  },
}));

vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: () => ({
    documentText: ideSessionMocks.documentText,
    setDocumentText: ideSessionMocks.setDocumentText,
  }),
}));

vi.mock('@monaco-editor/react', () => ({
  default: ({
    onMount,
    options,
  }: {
    onMount?: (editorInstance: editor.IStandaloneCodeEditor) => void;
    options?: editor.IStandaloneEditorConstructionOptions;
  }) => {
    onMount?.(monacoMocks.editorInstance as unknown as editor.IStandaloneCodeEditor);
    return <textarea aria-label={options?.ariaLabel ?? 'Active document'} readOnly />;
  },
}));

describe('IdeMonacoEditor', () => {
  beforeEach(() => {
    vi.clearAllMocks();
    monacoMocks.editorInstance.updateOptions.mockReset();
    monacoMocks.editorInstance.focus.mockReset();
    monacoMocks.editorInstance.getAction.mockReset();
    monacoMocks.editorInstance.hasTextFocus.mockReset();
    monacoMocks.editorInstance.getPosition.mockReset();
    monacoMocks.editorInstance.onDidChangeCursorPosition.mockReturnValue({ dispose: vi.fn() });
    monacoMocks.editorInstance.onDidFocusEditorWidget.mockReturnValue({ dispose: vi.fn() });
    monacoMocks.editorInstance.onDidBlurEditorWidget.mockReturnValue({ dispose: vi.fn() });
  });

  it('updates Monaco word wrap option when the View menu toggles it', () => {
    const { rerender } = render(<IdeMonacoEditor wordWrapEnabled />);

    expect(monacoMocks.editorInstance.updateOptions).toHaveBeenLastCalledWith({ wordWrap: 'on' });

    rerender(<IdeMonacoEditor wordWrapEnabled={false} />);

    expect(monacoMocks.editorInstance.updateOptions).toHaveBeenLastCalledWith({ wordWrap: 'off' });
  });

  it('runs Monaco find action for the find editor command', () => {
    const runFind = vi.fn();
    monacoMocks.editorInstance.getAction.mockReturnValue({ run: runFind });
    const initialCommand: IdeEditorCommand = { kind: 'findInEditor', nonce: 1 };
    const nextCommand: IdeEditorCommand = { kind: 'findInEditor', nonce: 2 };

    const { rerender } = render(<IdeMonacoEditor command={initialCommand} />);
    rerender(<IdeMonacoEditor command={nextCommand} />);

    expect(monacoMocks.editorInstance.focus).toHaveBeenCalled();
    expect(monacoMocks.editorInstance.getAction).toHaveBeenCalledWith('actions.find');
    expect(runFind).toHaveBeenCalledTimes(2);
  });
});
