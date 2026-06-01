import { describe, expect, it, vi, beforeEach } from 'vitest';

vi.mock('@infrastructure/tauri/bridge', () => ({
  readTextFile: vi.fn(),
  writeAppConfig: vi.fn(),
}));

import { readTextFile, writeAppConfig } from '@infrastructure/tauri/bridge';

import { loadTerminalHistory, saveTerminalHistory } from './terminalHistoryStore';

describe('terminalHistoryStore', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  it('loads and parses stored history', async () => {
    vi.mocked(readTextFile).mockResolvedValue('ls\npwd\n');
    await expect(loadTerminalHistory()).resolves.toEqual(['ls', 'pwd']);
    expect(readTextFile).toHaveBeenCalledWith('terminal_history.txt');
  });

  it('returns empty history when the file is missing or unreadable', async () => {
    vi.mocked(readTextFile).mockRejectedValue(new Error('not found'));
    await expect(loadTerminalHistory()).resolves.toEqual([]);
  });

  it('serializes history to the config file', async () => {
    vi.mocked(writeAppConfig).mockResolvedValue();
    await saveTerminalHistory(['ls', 'pwd']);
    expect(writeAppConfig).toHaveBeenCalledWith('terminal_history.txt', 'ls\npwd\n');
  });

  it('swallows write errors (persistence is best-effort)', async () => {
    vi.mocked(writeAppConfig).mockRejectedValue(new Error('disk full'));
    await expect(saveTerminalHistory(['ls'])).resolves.toBeUndefined();
  });
});
