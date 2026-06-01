import { beforeEach, describe, expect, it, vi } from 'vitest';

import { DEFAULT_APP_PREFERENCES } from '@domain/preferences/appPreferences';

import { disassemblerToolService } from './disassemblerToolService';

const { mockDisassembleWorkspaceFile, mockLoadPreferences } = vi.hoisted(() => ({
  mockDisassembleWorkspaceFile: vi.fn(),
  mockLoadPreferences: vi.fn(),
}));

vi.mock('@infrastructure/tauri/bridge', () => ({
  disassembleWorkspaceFile: mockDisassembleWorkspaceFile,
}));

vi.mock('@infrastructure/preferences/preferencesBridge', () => ({
  loadPreferences: mockLoadPreferences,
}));

describe('disassemblerToolService', () => {
  beforeEach(() => {
    mockDisassembleWorkspaceFile.mockReset();
    mockLoadPreferences.mockReset();
    mockLoadPreferences.mockResolvedValue({
      ...DEFAULT_APP_PREFERENCES,
      disassembly: {
        backend: 'objdump',
        objdumpPath: '/usr/bin/objdump',
        archHint: 'i386:x86-64',
        instructionLimit: 5000,
        syntax: 'intel',
      },
    });
    mockDisassembleWorkspaceFile.mockResolvedValue({
      executable: '/usr/bin/objdump',
      args: [],
      cwd: '/w',
      filePath: '/w/a.bin',
      stdout: '',
      stderr: '',
      statusCode: 0,
      sectionHeadersStdout: '',
      sectionHeadersStderr: '',
      sectionHeadersStatusCode: 0,
    });
  });

  it('passes persisted objdump settings to the tauri bridge', async () => {
    await disassemblerToolService('/w', '/w/a.bin');

    expect(mockDisassembleWorkspaceFile).toHaveBeenCalledWith('/w', '/w/a.bin', {
      objdumpPath: '/usr/bin/objdump',
      archHint: 'i386:x86-64',
      syntax: 'intel',
      instructionLimit: 5000,
    });
  });
});
