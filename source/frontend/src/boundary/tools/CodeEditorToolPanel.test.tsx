import { render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import type { IdeSessionContextValue } from '@boundary/workspace/IdeSessionContext';

vi.mock('@monaco-editor/react', () => ({
  default: ({
    value,
    onChange,
    options,
  }: {
    value?: string;
    onChange?: (v: string | undefined) => void;
    options?: { ariaLabel?: string; readOnly?: boolean };
  }) => (
    <textarea
      aria-label={options?.ariaLabel ?? 'Active document'}
      value={value ?? ''}
      readOnly={options?.readOnly === true}
      onChange={(e) => onChange?.(e.target.value)}
      spellCheck={false}
    />
  ),
}));

const { mockUseIdeSession } = vi.hoisted(() => ({
  mockUseIdeSession: vi.fn(),
}));

vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: mockUseIdeSession,
}));

import { CodeEditorToolPanel } from './CodeEditorToolPanel';

function stubSession(documentText: string): IdeSessionContextValue {
  return {
    activeFilePath: null,
    openFilePaths: [],
    pinnedFilePaths: new Set(),
    togglePinFilePath: vi.fn(),
    reorderOpenFiles: vi.fn(),
    documentText,
    dirtyFilePaths: [],
    activeDocumentDirty: false,
    activeFileIsBinary: false,
    setDocumentText: vi.fn(),
    openFileFromWorkspace: vi.fn(),
    openFileAtLine: vi.fn(),
    revealTarget: null,
    reloadCleanOpenBuffers: vi.fn(),
    openPanels: [],
    activePanel: null,
    previewFilePath: null,
    openPanel: vi.fn(),
    activatePanel: vi.fn(),
    closePanel: vi.fn(),
    activateOpenFile: vi.fn(),
    closeOpenFile: vi.fn(),
    closeOtherOpenFiles: vi.fn(),
    closeAllOpenFiles: vi.fn(),
    runFileMenuAction: vi.fn(),
    fileTreeRevision: 0,
    bumpFileTreeRevision: vi.fn(),
    fileContentRevision: 0,
    bumpFileContentRevision: vi.fn(),
  };
}

describe('CodeEditorToolPanel', () => {
  beforeEach(() => {
    mockUseIdeSession.mockReset();
  });

  it('passes documentText as value and readOnly to the editor', () => {
    const body = 'line one\nline two';
    mockUseIdeSession.mockReturnValue(stubSession(body));

    render(<CodeEditorToolPanel />);

    const editor = screen.getByRole('textbox', { name: /document mirror/i });
    expect(editor).toHaveValue(body);
    expect(editor).toHaveAttribute('readonly');
  });

  it('passes empty value when documentText is empty', () => {
    mockUseIdeSession.mockReturnValue(stubSession(''));

    render(<CodeEditorToolPanel />);

    const editor = screen.getByRole('textbox', { name: /document mirror/i });
    expect(editor).toHaveValue('');
    expect(editor).toHaveAttribute('readonly');
  });
});
