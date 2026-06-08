import { render, screen, waitFor, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';

import type { IdeSessionContextValue } from '@boundary/workspace/IdeSessionContext';
import type {
  DisassembleWorkspaceFile,
  DisassemblyCommandResult,
} from '@domain/disassembly/disassembly';

import { DisassemblerToolPanel } from '@plugins/tools/DisassemblerToolPanel';

const { mockUseIdeSession, mockUseWorkspaceRoot } = vi.hoisted(() => ({
  mockUseIdeSession: vi.fn(),
  mockUseWorkspaceRoot: vi.fn(),
}));

vi.mock('@boundary/workspace/IdeSessionContext', () => ({
  useIdeSession: mockUseIdeSession,
}));

vi.mock('@boundary/workspace/WorkspaceContext', () => ({
  useWorkspaceRoot: mockUseWorkspaceRoot,
}));

function stubSession(activeFilePath: string | null): IdeSessionContextValue {
  return {
    activeFilePath,
    openFilePaths: activeFilePath ? [activeFilePath] : [],
    pinnedFilePaths: new Set(),
    togglePinFilePath: vi.fn(),
    reorderOpenFiles: vi.fn(),
    documentText: '',
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

function disassemblyResult(
  overrides: Partial<DisassemblyCommandResult> = {},
): DisassemblyCommandResult {
  return {
    executable: 'objdump',
    args: ['-D', '-b', 'binary', '-m', 'i386:x86-64', '-M', 'intel', '/w/a.bin'],
    cwd: '/w',
    filePath: '/w/a.bin',
    stdout: `
Disassembly of section .text:

0000000000001040 <_start>:
    1040:\tf3 0f 1e fa          \tendbr64
    1044:\t31 ed                \txor    ebp,ebp

0000000000001130 <main>:
    1130:\t55                   \tpush   rbp

Disassembly of section .data:
    2000:\t00                   \tadd    BYTE PTR [rax],al
`,
    stderr: '',
    statusCode: 0,
    sectionHeadersStdout: `
Idx Name          Size      VMA               LMA               File off  Algn
 13 .text         00000121  0000000000001040  0000000000001040  00001040  2**4
 14 .data         00000010  0000000000002000  0000000000002000  00002000  2**4
`,
    sectionHeadersStderr: '',
    sectionHeadersStatusCode: 0,
    ...overrides,
  };
}

// Instruction cells syntax-highlight their text into per-token <span>s, so the
// mnemonic/operands are split across elements. Match the listing cell by its
// combined (whitespace-collapsed) text instead of a single text node.
function instr(text: string) {
  return (_content: string, element: Element | null): boolean =>
    element?.getAttribute('role') === 'cell' &&
    element.textContent?.replace(/\s+/g, ' ').trim() === text;
}

describe('DisassemblerToolPanel', () => {
  let disassembleFile: ReturnType<typeof vi.fn<DisassembleWorkspaceFile>>;
  let scrollTo: ReturnType<typeof vi.fn>;

  beforeAll(() => {
    if (globalThis.ResizeObserver === undefined) {
      globalThis.ResizeObserver = class {
        observe(): void {}
        unobserve(): void {}
        disconnect(): void {}
      } as typeof ResizeObserver;
    }
  });

  beforeEach(() => {
    disassembleFile = vi.fn<DisassembleWorkspaceFile>();
    // The listing jumps by scrolling its container (el.scrollTo), not the row's
    // scrollIntoView. jsdom implements neither, so stub the one the panel uses.
    scrollTo = vi.fn();
    Object.defineProperty(window.HTMLElement.prototype, 'scrollTo', {
      configurable: true,
      value: scrollTo,
    });
    mockUseIdeSession.mockReset();
    mockUseWorkspaceRoot.mockReset();
    mockUseIdeSession.mockImplementation(() => stubSession(null));
    mockUseWorkspaceRoot.mockReturnValue(null);
  });

  it('shows no active file message without invoking objdump', () => {
    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    expect(screen.getByText(/No file is active/i)).toBeInTheDocument();
    expect(disassembleFile).not.toHaveBeenCalled();
  });

  it('shows workspace required message when a file is active without a workspace', async () => {
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue(null);

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    await waitFor(() => {
      expect(
        screen.getByText('Open a workspace folder to disassemble the active file.'),
      ).toBeInTheDocument();
    });
    expect(disassembleFile).not.toHaveBeenCalled();
  });

  it('disassembles the active workspace file and renders parsed rows', async () => {
    disassembleFile.mockResolvedValueOnce(disassemblyResult());
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    expect(screen.getByText('Disassembling…')).toBeInTheDocument();

    await waitFor(() => {
      expect(screen.getByText('<_start>')).toBeInTheDocument();
    });

    expect(disassembleFile).toHaveBeenCalledWith('/w', '/w/a.bin');
    expect(screen.getByText('a.bin')).toBeInTheDocument();
    expect(screen.getByText('00001040')).toBeInTheDocument();
    expect(screen.getByText('f3 0f 1e fa')).toBeInTheDocument();
    expect(screen.getByText('endbr64')).toBeInTheDocument();
    expect(screen.getByText(instr('xor ebp,ebp'))).toBeInTheDocument();
    expect(screen.queryByText(/not implemented/i)).not.toBeInTheDocument();
    expect(screen.getByText('2 section(s)')).toBeInTheDocument();
    expect(screen.getByText('2 function label(s)')).toBeInTheDocument();
  });

  it('surfaces missing objdump bridge errors', async () => {
    disassembleFile.mockRejectedValueOnce(
      'objdump was not found. Install GNU binutils or LLVM tools, make sure objdump is available in PATH, and restart Cremniy. Custom objdump path support is pending settings work (MIG-017).',
    );
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    await waitFor(() => {
      expect(screen.getByRole('alert')).toHaveTextContent(/Custom objdump path support is pending settings work \(MIG-017\)/i);
    });
  });

  it('cancels an in-flight disassembly and ignores the late result', async () => {
    const user = userEvent.setup();
    let resolveDisassembly!: (result: DisassemblyCommandResult) => void;
    disassembleFile.mockReturnValueOnce(
      new Promise<DisassemblyCommandResult>((resolve) => {
        resolveDisassembly = resolve;
      }),
    );
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    expect(screen.getByText('Disassembling…')).toBeInTheDocument();
    await user.click(screen.getByRole('button', { name: 'Cancel' }));
    expect(screen.getByText('Disassembly cancelled.')).toBeInTheDocument();

    resolveDisassembly(disassemblyResult());

    await waitFor(() => {
      expect(screen.queryByText('endbr64')).not.toBeInTheDocument();
    });
  });

  it('filters rows by search text across address bytes mnemonic and operands', async () => {
    const user = userEvent.setup();
    disassembleFile.mockResolvedValueOnce(disassemblyResult());
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    await waitFor(() => {
      expect(screen.getByText(instr('xor ebp,ebp'))).toBeInTheDocument();
    });

    await user.type(screen.getByLabelText('Search'), '55');
    expect(screen.getByText(instr('push rbp'))).toBeInTheDocument();
    expect(screen.queryByText(instr('xor ebp,ebp'))).not.toBeInTheDocument();

    await user.clear(screen.getByLabelText('Search'));
    await user.type(screen.getByLabelText('Search'), '1044');
    expect(screen.getByText(instr('xor ebp,ebp'))).toBeInTheDocument();
    expect(screen.queryByText(instr('push rbp'))).not.toBeInTheDocument();

    await user.clear(screen.getByLabelText('Search'));
    await user.type(screen.getByLabelText('Search'), 'endbr64');
    expect(screen.getByText('endbr64')).toBeInTheDocument();
  });

  it('filters rows by selected section', async () => {
    const user = userEvent.setup();
    disassembleFile.mockResolvedValueOnce(disassemblyResult());
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    await waitFor(() => {
      expect(screen.getByText('endbr64')).toBeInTheDocument();
    });

    await user.selectOptions(screen.getByLabelText('Section'), '.data');

    expect(screen.getByText('Disassembly of section .data')).toBeInTheDocument();
    expect(screen.getByText(instr('add BYTE PTR [rax],al'))).toBeInTheDocument();
    expect(screen.queryByText('endbr64')).not.toBeInTheDocument();
  });

  it('shows and clears the diagnostic log', async () => {
    const user = userEvent.setup();
    disassembleFile.mockResolvedValueOnce(
      disassemblyResult({
        stderr: 'objdump warning: truncated input',
        statusCode: 1,
      }),
    );
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    await waitFor(() => {
      expect(screen.getByText('objdump warning: truncated input')).toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: 'Show diagnostic log' }));
    expect(screen.getByLabelText('Diagnostic log')).toHaveTextContent('Command');
    expect(screen.getByLabelText('Diagnostic log')).toHaveTextContent(
      'objdump warning: truncated input',
    );

    await user.click(screen.getByRole('button', { name: 'Clear diagnostic log' }));
    expect(screen.getByLabelText('Diagnostic log')).toHaveTextContent('No diagnostic entries.');
  });

  it('jumps to a function label when function metadata is available', async () => {
    const user = userEvent.setup();
    disassembleFile.mockResolvedValueOnce(disassemblyResult());
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    await waitFor(() => {
      expect(screen.getByText('<main>')).toBeInTheDocument();
    });

    await user.type(screen.getByLabelText('Search'), '55');
    await user.selectOptions(screen.getByLabelText('Function'), '0000000000001130');

    await waitFor(() => {
      expect(scrollTo).toHaveBeenCalled();
    });
    expect(screen.getByLabelText('Search')).toHaveValue('');
    expect(screen.getByLabelText('Section')).toHaveValue('.text');
    expect(screen.getByText('<main>')).toBeInTheDocument();
  });

  it('shows ported instruction help when a known instruction row is clicked', async () => {
    const user = userEvent.setup();
    disassembleFile.mockResolvedValueOnce(disassemblyResult());
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    const pushCell = await screen.findByText(instr('push rbp'));
    await user.click(pushCell);

    const details = await screen.findByLabelText('Instruction details');
    expect(within(details).getByText(/PUSH/)).toBeInTheDocument();
    expect(within(details).getByText(/не изменяет/)).toBeInTheDocument();
  });

  it('opens a hex patch box for an instruction with a file offset', async () => {
    const user = userEvent.setup();
    disassembleFile.mockResolvedValueOnce(disassemblyResult());
    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<DisassemblerToolPanel disassembleFile={disassembleFile} />);

    const endbrCell = await screen.findByText('endbr64');
    await user.click(endbrCell);

    const details = await screen.findByLabelText('Instruction details');
    expect(within(details).getByLabelText('Hex patch')).toBeInTheDocument();
  });
});
