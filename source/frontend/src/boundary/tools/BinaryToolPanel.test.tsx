import { act, render, screen, waitFor, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { beforeAll, beforeEach, describe, expect, it, vi } from 'vitest';

import type { IdeSessionContextValue } from '@boundary/workspace/IdeSessionContext';
import { DEFAULT_APP_PREFERENCES } from '@domain/preferences/appPreferences';
import { readWorkspaceFileBytes, writeWorkspaceFileBytes } from '@infrastructure/tauri/bridge';

import { BinaryToolPanel } from '@plugins/tools/BinaryToolPanel';
import panelStyles from '@plugins/tools/BinaryToolPanel.module.css';

const { mockUseIdeSession, mockUseWorkspaceRoot, mockLoadPreferences, mockSavePreferences } =
  vi.hoisted(() => ({
    mockUseIdeSession: vi.fn(),
    mockUseWorkspaceRoot: vi.fn(),
    mockLoadPreferences: vi.fn(),
    mockSavePreferences: vi.fn(),
  }));

vi.mock('@infrastructure/tauri/bridge', () => ({
  readWorkspaceFileBytes: vi.fn(),
  writeWorkspaceFileBytes: vi.fn(),
}));

// BinaryToolPanel reads hex layout prefs on mount; stub the bridge so it doesn't
// hit the (unavailable) Tauri invoke and reject in the background.
vi.mock('@infrastructure/preferences/preferencesBridge', () => ({
  loadPreferences: mockLoadPreferences,
  savePreferences: mockSavePreferences,
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

function createDeferred<T>(): {
  promise: Promise<T>;
  resolve: (value: T | PromiseLike<T>) => void;
  reject: (reason?: unknown) => void;
} {
  let resolve!: (value: T | PromiseLike<T>) => void;
  let reject!: (reason?: unknown) => void;
  const promise = new Promise<T>((promiseResolve, promiseReject) => {
    resolve = promiseResolve;
    reject = promiseReject;
  });
  return { promise, resolve, reject };
}

function getControlledPanel(tab: HTMLElement): HTMLElement {
  const panelId = tab.getAttribute('aria-controls');
  expect(panelId).toBeTruthy();

  const panel = document.getElementById(panelId!);
  expect(panel).not.toBeNull();

  return panel!;
}

describe('BinaryToolPanel', () => {
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
    vi.mocked(readWorkspaceFileBytes).mockReset();
    // Default to a resolved (empty) buffer so panels that load on mount don't
    // call `.then` on undefined; value-specific tests override this.
    vi.mocked(readWorkspaceFileBytes).mockResolvedValue(new Uint8Array());
    vi.mocked(writeWorkspaceFileBytes).mockReset();
    vi.mocked(writeWorkspaceFileBytes).mockResolvedValue(undefined);
    // The panel reads hex-layout prefs on mount via loadPreferences().then(...);
    // give it a resolved value so the mount doesn't `.then` on undefined.
    mockLoadPreferences.mockReset();
    mockLoadPreferences.mockResolvedValue(DEFAULT_APP_PREFERENCES);
    mockSavePreferences.mockReset();
    mockSavePreferences.mockResolvedValue(undefined);
    mockUseIdeSession.mockReset();
    mockUseWorkspaceRoot.mockReset();
    mockUseIdeSession.mockImplementation(() => stubSession(null));
    mockUseWorkspaceRoot.mockReturnValue(null);
  });

  it('shows no active file message when there is no active file', () => {
    mockUseIdeSession.mockReturnValue(stubSession(null));
    render(<BinaryToolPanel />);

    expect(
      screen.getByText(/No file is active\. Open a file from the workspace tree/i),
    ).toBeInTheDocument();
    expect(readWorkspaceFileBytes).not.toHaveBeenCalled();
  });

  it('shows workspace required message when a file is active but workspace root is missing', async () => {
    mockUseIdeSession.mockReturnValue(stubSession('/proj/sample.bin'));
    mockUseWorkspaceRoot.mockReturnValue(null);

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(
        screen.getByText('Open a workspace folder to load binary content.'),
      ).toBeInTheDocument();
    });
    expect(readWorkspaceFileBytes).not.toHaveBeenCalled();
  });

  it('shows loading then hex and ASCII after bytes resolve', async () => {
    let resolveBytes!: (data: Uint8Array) => void;
    const bytesPromise = new Promise<Uint8Array>((resolve) => {
      resolveBytes = resolve;
    });

    vi.mocked(readWorkspaceFileBytes).mockReturnValue(bytesPromise);

    mockUseIdeSession.mockReturnValue(stubSession('/w/a.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    expect(screen.getByText('Loading…')).toBeInTheDocument();

    resolveBytes(new Uint8Array([0x41, 0x42, 0x03]));

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    expect(readWorkspaceFileBytes).toHaveBeenCalledWith('/w', '/w/a.bin');
    // Byte values render as editable cells; query by the cell aria-label so the
    // assertion targets the hex grid (the column-header row also shows "03").
    expect(
      screen.getByRole('button', { name: 'Edit byte at offset 00000000, current value 41' }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole('button', { name: 'Edit byte at offset 00000001, current value 42' }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole('button', { name: 'Edit byte at offset 00000002, current value 03' }),
    ).toBeInTheDocument();
    expect(screen.getByText('00000000')).toBeInTheDocument();
    const region = screen.getByRole('region', { name: 'Binary tool' });
    expect(region.textContent).toMatch(/AB\./);
  });

  it('shows binary format tabs and PE header details', async () => {
    const user = userEvent.setup();
    const bytes = new Uint8Array(160);
    bytes.set([0x4d, 0x5a]);
    bytes.set([0x80, 0x00, 0x00, 0x00], 0x3c);
    bytes.set([0x50, 0x45, 0x00, 0x00], 0x80);
    bytes.set([0x64, 0x86], 0x84);
    bytes.set([0x02, 0x00], 0x86);
    bytes.set([0xf0, 0x00], 0x94);
    bytes.set([0x0b, 0x02], 0x98);
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(bytes);

    mockUseIdeSession.mockReturnValue(stubSession('/w/app.exe'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.getByText('Detected: PE')).toBeInTheDocument();
    });

    const rawTab = screen.getByRole('tab', { name: 'RAW' });
    const elfTab = screen.getByRole('tab', { name: 'ELF' });
    const peTab = screen.getByRole('tab', { name: 'PE' });
    const mbrTab = screen.getByRole('tab', { name: 'MBR' });
    const rawPanel = getControlledPanel(rawTab);
    const elfPanel = getControlledPanel(elfTab);
    const pePanel = getControlledPanel(peTab);
    const mbrPanel = getControlledPanel(mbrTab);

    expect(rawTab).toHaveAttribute('aria-selected', 'true');
    expect(rawTab).toHaveAttribute('tabindex', '0');
    expect(elfTab).toHaveAttribute('tabindex', '-1');
    expect(peTab).toHaveAttribute('tabindex', '-1');
    expect(mbrTab).toHaveAttribute('tabindex', '-1');
    expect(rawPanel).not.toHaveAttribute('hidden');
    expect(elfPanel).toHaveAttribute('hidden');
    expect(pePanel).toHaveAttribute('hidden');
    expect(mbrPanel).toHaveAttribute('hidden');

    await user.click(peTab);

    expect(screen.getByText('Portable Executable header detected.')).toBeInTheDocument();
    expect(screen.getByText('x86-64 (AMD64)')).toBeInTheDocument();
    expect(screen.getByText('PE32+ (0x020B)')).toBeInTheDocument();

    expect(peTab).toHaveAttribute('aria-selected', 'true');
    expect(peTab).toHaveAttribute('tabindex', '0');
    expect(pePanel).not.toHaveAttribute('hidden');
    expect(peTab).toHaveAttribute('aria-controls', pePanel.id);
    expect(pePanel).toHaveAttribute('aria-labelledby', peTab.id);

    await user.keyboard('{ArrowRight}');
    expect(mbrTab).toHaveFocus();
    expect(mbrTab).toHaveAttribute('aria-selected', 'true');
    expect(mbrTab).toHaveAttribute('tabindex', '0');

    await user.keyboard('{Home}');
    expect(rawTab).toHaveFocus();
    expect(rawTab).toHaveAttribute('aria-selected', 'true');

    await user.keyboard('{End}');
    expect(mbrTab).toHaveFocus();
    expect(mbrTab).toHaveAttribute('aria-selected', 'true');

    await user.keyboard('{ArrowLeft}');
    expect(peTab).toHaveFocus();
    expect(peTab).toHaveAttribute('aria-selected', 'true');
  });

  it('updates the active format panel from the in-progress edited byte', async () => {
    const user = userEvent.setup();
    const bytes = new Uint8Array(160);
    bytes.set([0x00, 0x5a]);
    bytes.set([0x80, 0x00, 0x00, 0x00], 0x3c);
    bytes.set([0x50, 0x45, 0x00, 0x00], 0x80);
    bytes.set([0x4c, 0x01], 0x84);
    bytes.set([0x01, 0x00], 0x86);
    bytes.set([0x02, 0x00], 0x94);
    bytes.set([0x0b, 0x01], 0x98);
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(bytes);

    mockUseIdeSession.mockReturnValue(stubSession('/w/editing.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.getByText('Detected: Unknown')).toBeInTheDocument();
    });

    await user.click(screen.getByRole('tab', { name: 'PE' }));
    expect(screen.getByText('No DOS MZ signature.')).toBeInTheDocument();

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000000, current value 00',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000000'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000000'), '4d');

    expect(screen.getByText('Detected: PE')).toBeInTheDocument();
    expect(screen.getByText('Portable Executable header detected.')).toBeInTheDocument();
    expect(screen.getByText('x86 (i386)')).toBeInTheDocument();
    expect(screen.getByText('PE32 (0x010B)')).toBeInTheDocument();
  });

  it('shows empty file status when bytes resolve to an empty Uint8Array', async () => {
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array());

    mockUseIdeSession.mockReturnValue(stubSession('/w/empty.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.getByText('This file is empty (0 bytes).')).toBeInTheDocument();
    });
    expect(screen.getByText('Detected: Unknown')).toBeInTheDocument();
    expect(screen.getByText('0 bytes')).toBeInTheDocument();
    expect(readWorkspaceFileBytes).toHaveBeenCalledWith('/w', '/w/empty.bin');
  });

  it('shows invoke error in the alert region', async () => {
    vi.mocked(readWorkspaceFileBytes).mockRejectedValueOnce(new Error('read failed'));

    mockUseIdeSession.mockReturnValue(stubSession('/w/x.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.getByRole('alert')).toHaveTextContent('read failed');
    });
  });

  it('opens Find/go dialog and scrolls viewport after go to offset', async () => {
    const user = userEvent.setup();
    const bytes = new Uint8Array(256);
    bytes.fill(0x41);
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(bytes);

    mockUseIdeSession.mockReturnValue(stubSession('/w/large.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: /Find \/ go to/i }));

    expect(
      screen.getByRole('dialog', { name: /Find \/ replace \/ go to offset/i }),
    ).toBeInTheDocument();

    await user.clear(screen.getByLabelText('Offset'));
    await user.type(screen.getByLabelText('Offset'), '80');
    await user.click(screen.getByRole('button', { name: 'Go' }));

    const section = screen.getByRole('region', { name: 'Binary tool' });
    const scroller = section.querySelector(`.${panelStyles.scroll}`);
    expect(scroller).not.toBeNull();
    expect((scroller as HTMLDivElement).scrollTop).toBe(8 * 18);
    expect(screen.getByText('00000080')).toBeInTheDocument();
  });

  it('shows not found in Find/go dialog when pattern is absent', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42, 0x43]));

    mockUseIdeSession.mockReturnValue(stubSession('/w/abc.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(screen.getByRole('button', { name: /Find \/ go to/i }));

    await user.type(screen.getByLabelText('Find'), 'ff');
    await user.click(screen.getByRole('button', { name: 'Find next' }));

    const dialog = screen.getByRole('dialog', { name: /Find \/ replace \/ go to offset/i });
    expect(within(dialog).getByRole('status')).toHaveTextContent('Not found.');
  });

  it('edits a single byte inline and marks the buffer dirty', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42, 0x43]));

    mockUseIdeSession.mockReturnValue(stubSession('/w/abc.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 42',
      }),
    );
    const input = screen.getByLabelText('Hex byte at offset 00000001');
    await user.clear(input);
    await user.type(input, 'ff');
    await user.keyboard('{Enter}');

    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value ff',
      }),
    ).toBeInTheDocument();
    expect(screen.getByText('Unsaved changes')).toBeInTheDocument();
  });

  it('rejects invalid hex input with an accessible error', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42]));

    mockUseIdeSession.mockReturnValue(stubSession('/w/ab.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000000, current value 41',
      }),
    );
    const input = screen.getByLabelText('Hex byte at offset 00000000');
    await user.clear(input);
    await user.type(input, 'zz');
    await user.keyboard('{Enter}');

    expect(screen.getByRole('alert')).toHaveTextContent('Enter exactly two hex digits (00-ff).');
    expect(input).toHaveAttribute('aria-invalid', 'true');
    expect(screen.getByText('Saved')).toBeInTheDocument();
    expect(writeWorkspaceFileBytes).not.toHaveBeenCalled();
  });

  it('keeps edited bytes dirty and shows an error when save fails', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42, 0x43]));
    vi.mocked(writeWorkspaceFileBytes).mockRejectedValueOnce(new Error('save failed'));

    mockUseIdeSession.mockReturnValue(stubSession('/w/abc.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 42',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000001'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000001'), '44');
    await user.keyboard('{Enter}');
    await user.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => {
      expect(screen.getByRole('alert')).toHaveTextContent('save failed');
    });
    expect(screen.getByText('Unsaved changes')).toBeInTheDocument();
    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 44',
      }),
    ).toBeInTheDocument();
  });

  it('resets an active edited cell back to the saved byte', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42, 0x43]));

    mockUseIdeSession.mockReturnValue(stubSession('/w/abc.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 42',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000001'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000001'), '44');
    await user.click(screen.getByRole('button', { name: 'Reset' }));

    expect(screen.queryByLabelText('Hex byte at offset 00000001')).not.toBeInTheDocument();
    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 42',
      }),
    ).toBeInTheDocument();
    expect(screen.getByText('Saved')).toBeInTheDocument();
    expect(writeWorkspaceFileBytes).not.toHaveBeenCalled();
  });

  it('edits a byte at a row boundary without changing the next row', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(
      new Uint8Array([
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11,
      ]),
    );

    mockUseIdeSession.mockReturnValue(stubSession('/w/boundary.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 0000000f, current value 0f',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 0000000f'));
    await user.type(screen.getByLabelText('Hex byte at offset 0000000f'), 'aa');
    await user.keyboard('{Enter}');

    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 0000000f, current value aa',
      }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000010, current value 10',
      }),
    ).toBeInTheDocument();
    expect(screen.getByText('Unsaved changes')).toBeInTheDocument();
  });

  it('saves the current edited buffer snapshot before the cell is committed', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42, 0x43]));

    mockUseIdeSession.mockReturnValue(stubSession('/w/abc.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 42',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000001'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000001'), '44');
    await user.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => {
      expect(writeWorkspaceFileBytes).toHaveBeenCalledWith(
        '/w',
        '/w/abc.bin',
        new Uint8Array([0x41, 0x44, 0x43]),
      );
    });
    expect(screen.queryByLabelText('Hex byte at offset 00000001')).not.toBeInTheDocument();
    expect(screen.getByText('Saved.')).toBeInTheDocument();
  });

  it('keeps the new active file buffer when a previous file save resolves late', async () => {
    const user = userEvent.setup();
    const saveDeferred = createDeferred<void>();
    let activeFilePath = '/w/old.bin';

    vi.mocked(readWorkspaceFileBytes).mockImplementation((_workspacePath, filePath) => {
      if (filePath === '/w/old.bin') {
        return Promise.resolve(new Uint8Array([0x41, 0x42]));
      }
      if (filePath === '/w/new.bin') {
        return Promise.resolve(new Uint8Array([0x50, 0x51]));
      }
      return Promise.resolve(new Uint8Array());
    });
    vi.mocked(writeWorkspaceFileBytes).mockReturnValueOnce(saveDeferred.promise);

    mockUseIdeSession.mockImplementation(() => stubSession(activeFilePath));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    const { rerender } = render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(
        screen.getByRole('button', {
          name: 'Edit byte at offset 00000001, current value 42',
        }),
      ).toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 42',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000001'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000001'), '44');
    await user.keyboard('{Enter}');
    await user.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => {
      expect(writeWorkspaceFileBytes).toHaveBeenCalledWith(
        '/w',
        '/w/old.bin',
        new Uint8Array([0x41, 0x44]),
      );
    });

    activeFilePath = '/w/new.bin';
    rerender(<BinaryToolPanel />);

    await waitFor(() => {
      expect(
        screen.getByRole('button', {
          name: 'Edit byte at offset 00000000, current value 50',
        }),
      ).toBeInTheDocument();
    });

    await act(async () => {
      saveDeferred.resolve();
      await saveDeferred.promise;
    });

    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000000, current value 50',
      }),
    ).toBeInTheDocument();
    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 51',
      }),
    ).toBeInTheDocument();
    expect(
      screen.queryByRole('button', {
        name: 'Edit byte at offset 00000001, current value 44',
      }),
    ).not.toBeInTheDocument();
    expect(screen.queryByText('Saved.')).not.toBeInTheDocument();
  });

  it('saves edited bytes and resets to the saved baseline', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42, 0x43]));

    mockUseIdeSession.mockReturnValue(stubSession('/w/abc.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 42',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000001'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000001'), '44');
    await user.keyboard('{Enter}');
    await user.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => {
      expect(writeWorkspaceFileBytes).toHaveBeenCalledWith(
        '/w',
        '/w/abc.bin',
        new Uint8Array([0x41, 0x44, 0x43]),
      );
    });
    expect(screen.getByText('Saved.')).toBeInTheDocument();

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 44',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000001'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000001'), '45');
    await user.keyboard('{Enter}');
    await user.click(screen.getByRole('button', { name: 'Reset' }));

    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000001, current value 44',
      }),
    ).toBeInTheDocument();
    expect(screen.getByText('Saved')).toBeInTheDocument();
  });

  it('finds bytes from the current edited buffer', async () => {
    const user = userEvent.setup();
    vi.mocked(readWorkspaceFileBytes).mockResolvedValueOnce(new Uint8Array([0x41, 0x42, 0x43]));

    mockUseIdeSession.mockReturnValue(stubSession('/w/abc.bin'));
    mockUseWorkspaceRoot.mockReturnValue({ path: '/w' });

    render(<BinaryToolPanel />);

    await waitFor(() => {
      expect(screen.queryByText('Loading…')).not.toBeInTheDocument();
    });

    await user.click(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000002, current value 43',
      }),
    );
    await user.clear(screen.getByLabelText('Hex byte at offset 00000002'));
    await user.type(screen.getByLabelText('Hex byte at offset 00000002'), 'ff');
    await user.keyboard('{Enter}');

    await user.click(screen.getByRole('button', { name: /Find \/ go to/i }));
    // Default search mode is Text; pick Hex so "ff" matches the byte 0xff. The
    // search runs against the current (committed-edit) buffer, including 0xff.
    await user.click(
      within(screen.getByRole('group', { name: 'Search mode' })).getByRole('radio', {
        name: 'Hex',
      }),
    );
    await user.type(screen.getByLabelText('Find'), 'ff');
    await user.click(screen.getByRole('button', { name: 'Find next' }));

    expect(
      screen.getByRole('button', {
        name: 'Edit byte at offset 00000002, current value ff',
      }),
    ).toHaveClass(panelStyles.hexByteHighlight);
  });
});
