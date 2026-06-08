import { render } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { editor } from 'monaco-editor';

import { IdeMonacoEditor, type IdeEditorCommand } from './IdeMonacoEditor';

const ideSessionMocks = vi.hoisted(() => ({
  documentText: 'body',
  setDocumentText: vi.fn(),
  activeFilePath: '/proj/main.ts' as string | null,
  revealTarget: null as { path: string; line: number; nonce: number } | null,
}));

const toolDockMocks = vi.hoisted(() => ({
  selectToolTab: vi.fn(),
}));

const monacoMocks = vi.hoisted(() => ({
  editorInstance: {
    updateOptions: vi.fn(),
    focus: vi.fn(),
    getAction: vi.fn(),
    hasTextFocus: vi.fn(),
    getPosition: vi.fn(),
    // The model is updated for tab width / insert-spaces on mount + file change.
    getModel: vi.fn(() => ({ updateOptions: vi.fn() })),
    getRawOptions: vi.fn(() => ({ fontSize: 14 })),
    revealLineInCenter: vi.fn(),
    setPosition: vi.fn(),
    onDidChangeCursorPosition: vi.fn(),
    onDidFocusEditorWidget: vi.fn(),
    onDidBlurEditorWidget: vi.fn(),
    // Wired in onMount to persist Ctrl+wheel zoom back into preferences.
    onDidChangeConfiguration: vi.fn(),
  },
}));

vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: () => ({
    documentText: ideSessionMocks.documentText,
    setDocumentText: ideSessionMocks.setDocumentText,
    activeFilePath: ideSessionMocks.activeFilePath,
    revealTarget: ideSessionMocks.revealTarget,
  }),
}));

vi.mock('@boundary/workspace/ToolDockContext', () => ({
  useToolDock: () => ({
    selectToolTab: toolDockMocks.selectToolTab,
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
    monacoMocks.editorInstance.getModel.mockReturnValue({ updateOptions: vi.fn() });
    monacoMocks.editorInstance.getRawOptions.mockReturnValue({ fontSize: 14 });
    monacoMocks.editorInstance.onDidChangeCursorPosition.mockReturnValue({ dispose: vi.fn() });
    monacoMocks.editorInstance.onDidFocusEditorWidget.mockReturnValue({ dispose: vi.fn() });
    monacoMocks.editorInstance.onDidBlurEditorWidget.mockReturnValue({ dispose: vi.fn() });
    monacoMocks.editorInstance.onDidChangeConfiguration.mockReturnValue({ dispose: vi.fn() });
    toolDockMocks.selectToolTab.mockReset();
    ideSessionMocks.setDocumentText.mockReset();
    ideSessionMocks.documentText = 'body';
    ideSessionMocks.activeFilePath = '/proj/main.ts';
    ideSessionMocks.revealTarget = null;
  });

  it('updates Monaco word wrap option when the View menu toggles it', () => {
    // A dedicated effect mirrors the wordWrapEnabled prop into Monaco via
    // updateOptions({ wordWrap }). (updateOptions is also called for fontSize /
    // on mount, so assert on the wordWrap call specifically rather than "last".)
    const { rerender } = render(<IdeMonacoEditor wordWrapEnabled />);

    expect(monacoMocks.editorInstance.updateOptions).toHaveBeenCalledWith({ wordWrap: 'on' });

    rerender(<IdeMonacoEditor wordWrapEnabled={false} />);

    expect(monacoMocks.editorInstance.updateOptions).toHaveBeenCalledWith({ wordWrap: 'off' });
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
