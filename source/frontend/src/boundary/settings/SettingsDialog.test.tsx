import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { beforeEach, describe, expect, it, vi } from 'vitest';

import {
  DEFAULT_APP_PREFERENCES,
  type AppPreferences,
  type DisassemblyPreferences,
} from '@domain/preferences/appPreferences';
import type { SettingsService } from '@domain/preferences/settingsService';

import { SettingsDialog } from './SettingsDialog';

const mockLoadPreferences = vi.fn<SettingsService['loadPreferences']>();
const mockSavePreferences = vi.fn<SettingsService['savePreferences']>();
const mockTestObjdumpTool = vi.fn<SettingsService['testObjdumpTool']>();
const mockExportPreferences = vi.fn<SettingsService['exportPreferences']>();
const mockImportPreferences = vi.fn<SettingsService['importPreferences']>();

const service: SettingsService = {
  loadPreferences: mockLoadPreferences,
  savePreferences: mockSavePreferences,
  testObjdumpTool: mockTestObjdumpTool,
  exportPreferences: mockExportPreferences,
  importPreferences: mockImportPreferences,
};

function preferences(
  overrides: Partial<Omit<AppPreferences, 'disassembly'>> & {
    disassembly?: Partial<DisassemblyPreferences>;
  } = {},
): AppPreferences {
  return {
    ...DEFAULT_APP_PREFERENCES,
    ...overrides,
    disassembly: {
      ...DEFAULT_APP_PREFERENCES.disassembly,
      ...overrides.disassembly,
    },
  };
}

describe('SettingsDialog', () => {
  beforeEach(() => {
    mockLoadPreferences.mockReset();
    mockSavePreferences.mockReset();
    mockTestObjdumpTool.mockReset();
    mockExportPreferences.mockReset();
    mockImportPreferences.mockReset();
    mockLoadPreferences.mockResolvedValue(preferences());
    mockSavePreferences.mockResolvedValue(undefined);
    mockTestObjdumpTool.mockResolvedValue('objdump OK');
    mockExportPreferences.mockResolvedValue('/docs/cremniy-settings.json');
    mockImportPreferences.mockResolvedValue(null);
  });

  it('loads and persists disassembly tooling settings', async () => {
    const user = userEvent.setup();
    const onSaved = vi.fn();
    mockLoadPreferences.mockResolvedValue(
      preferences({
        theme: 'dark',
        terminalPanelVisible: true,
        disassembly: {
          backend: 'objdump',
          objdumpPath: '/usr/bin/objdump',
          archHint: 'i386',
          instructionLimit: 1500,
          syntax: 'intel',
        },
      }),
    );

    render(<SettingsDialog open onClose={vi.fn()} onSaved={onSaved} service={service} />);

    await user.click(screen.getByRole('button', { name: 'Disassembly' }));
    expect(await screen.findByLabelText('objdump path')).toHaveValue('/usr/bin/objdump');
    await user.clear(screen.getByLabelText('objdump path'));
    await user.type(screen.getByLabelText('objdump path'), '/opt/bin/objdump');
    await user.clear(screen.getByLabelText('Architecture hint'));
    await user.type(screen.getByLabelText('Architecture hint'), 'arm');
    await user.clear(screen.getByLabelText('Instruction/render limit'));
    await user.type(screen.getByLabelText('Instruction/render limit'), '3000');
    await user.selectOptions(screen.getByLabelText('Syntax preference'), 'att');
    await user.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => {
      expect(mockSavePreferences).toHaveBeenCalledWith(
        expect.objectContaining({
          disassembly: {
            backend: 'objdump',
            objdumpPath: '/opt/bin/objdump',
            archHint: 'arm',
            instructionLimit: 3000,
            syntax: 'att',
            radare2Path: '',
            radare2AnalysisLevel: 'none',
            radare2PreCommands: '',
          },
        }),
      );
    });
    expect(onSaved).toHaveBeenCalledWith(
      expect.objectContaining({
        disassembly: expect.objectContaining({ archHint: 'arm' }),
      }),
    );
  });

  it('normalizes disassembly settings before saving', async () => {
    const user = userEvent.setup();
    render(<SettingsDialog open onClose={vi.fn()} service={service} />);

    await user.click(screen.getByRole('button', { name: 'Disassembly' }));
    await screen.findByLabelText('Instruction/render limit');
    await user.clear(screen.getByLabelText('objdump path'));
    await user.type(screen.getByLabelText('objdump path'), '  /opt/bin/objdump  ');
    await user.clear(screen.getByLabelText('Architecture hint'));
    await user.type(screen.getByLabelText('Architecture hint'), '  i386:x86-64  ');
    await user.clear(screen.getByLabelText('Instruction/render limit'));
    await user.type(screen.getByLabelText('Instruction/render limit'), '1');
    await user.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => {
      expect(mockSavePreferences).toHaveBeenCalledWith(
        expect.objectContaining({
          disassembly: expect.objectContaining({
            objdumpPath: '/opt/bin/objdump',
            archHint: 'i386:x86-64',
            instructionLimit: 50,
          }),
        }),
      );
    });
  });

  it('tests objdump with the current path and workspace', async () => {
    const user = userEvent.setup();
    mockLoadPreferences.mockResolvedValue(
      preferences({
        disassembly: {
          backend: 'objdump',
          objdumpPath: '/usr/bin/objdump',
          archHint: '',
          instructionLimit: 2000,
          syntax: 'intel',
        },
      }),
    );
    mockTestObjdumpTool.mockResolvedValue('objdump OK: /usr/bin/objdump');

    render(<SettingsDialog open onClose={vi.fn()} workspaceRoot="/workspace" service={service} />);

    await user.click(screen.getByRole('button', { name: 'Disassembly' }));
    expect(await screen.findByLabelText('objdump path')).toHaveValue('/usr/bin/objdump');
    await user.click(screen.getByRole('button', { name: 'Self-check' }));

    await waitFor(() => {
      expect(mockTestObjdumpTool).toHaveBeenCalledWith('/workspace', '/usr/bin/objdump');
    });
    expect(screen.getByText('objdump OK: /usr/bin/objdump')).toBeInTheDocument();
  });

  it('disables actions while testing objdump and surfaces failures', async () => {
    const user = userEvent.setup();
    let rejectObjdump!: (error: Error) => void;
    mockTestObjdumpTool.mockImplementationOnce(
      () =>
        new Promise<string>((_resolve, reject) => {
          rejectObjdump = reject;
        }),
    );

    render(<SettingsDialog open onClose={vi.fn()} workspaceRoot="/workspace" service={service} />);

    await user.click(screen.getByRole('button', { name: 'Disassembly' }));
    const testButton = await screen.findByRole('button', { name: 'Self-check' });
    await waitFor(() => {
      expect(testButton).not.toBeDisabled();
    });
    await user.click(testButton);

    expect(mockTestObjdumpTool).toHaveBeenCalledWith('/workspace', '');
    await waitFor(() => {
      expect(testButton).toBeDisabled();
      expect(screen.getByRole('button', { name: 'Save' })).toBeDisabled();
    });

    rejectObjdump(new Error('objdump path is not executable'));

    expect(await screen.findByText('objdump path is not executable')).toBeInTheDocument();
    expect(testButton).not.toBeDisabled();
  });

  it('exports settings to a file and reports the path', async () => {
    const user = userEvent.setup();
    render(<SettingsDialog open onClose={vi.fn()} service={service} />);

    const exportButton = await screen.findByRole('button', { name: 'Export…' });
    await waitFor(() => expect(exportButton).not.toBeDisabled());
    await user.click(exportButton);

    await waitFor(() => {
      expect(mockExportPreferences).toHaveBeenCalled();
      expect(screen.getByText(/Settings exported to/)).toBeInTheDocument();
    });
  });

  it('imports settings and reflects them into the dialog', async () => {
    const user = userEvent.setup();
    mockImportPreferences.mockResolvedValue(
      preferences({
        theme: 'light',
        disassembly: {
          backend: 'objdump',
          objdumpPath: '/imported/objdump',
          archHint: 'arm',
          instructionLimit: 1234,
          syntax: 'att',
        },
      }),
    );
    const onSaved = vi.fn();
    render(<SettingsDialog open onClose={vi.fn()} onSaved={onSaved} service={service} />);

    await user.click(screen.getByRole('button', { name: 'Disassembly' }));
    await screen.findByLabelText('objdump path');
    await user.click(screen.getByRole('button', { name: 'Import…' }));

    await waitFor(() => {
      expect(screen.getByLabelText('objdump path')).toHaveValue('/imported/objdump');
      expect(screen.getByLabelText('Architecture hint')).toHaveValue('arm');
    });
    expect(onSaved).toHaveBeenCalledWith(
      expect.objectContaining({ disassembly: expect.objectContaining({ archHint: 'arm' }) }),
    );
  });

  it('persists editor word wrap preference', async () => {
    const user = userEvent.setup();

    mockLoadPreferences.mockResolvedValue(preferences({ editorWordWrap: true }));

    render(<SettingsDialog open onClose={vi.fn()} service={service} />);

    await user.click(screen.getByRole('button', { name: 'Editor' }));
    const wrap = await screen.findByRole('checkbox', { name: /word wrap in editor/i });
    await user.click(wrap);
    await user.click(screen.getByRole('button', { name: 'Save' }));

    await waitFor(() => {
      expect(mockSavePreferences).toHaveBeenCalledWith(expect.objectContaining({ editorWordWrap: false }));
    });
  });
});
