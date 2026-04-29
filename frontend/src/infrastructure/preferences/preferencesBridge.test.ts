import { describe, it, expect, vi, beforeEach } from 'vitest';

vi.mock('@tauri-apps/api/core', () => ({
  invoke: vi.fn(),
}));

import { invoke } from '@tauri-apps/api/core';
import { DEFAULT_APP_PREFERENCES } from '@domain/preferences/appPreferences';

import { loadPreferences, savePreferences } from './preferencesBridge';

describe('preferencesBridge', () => {
  beforeEach(() => {
    vi.mocked(invoke).mockReset();
  });

  it('loadPreferences parses Rust JSON envelope', async () => {
    const json = '{"theme":"light","recentWorkspacePaths":["/a"]}\n';
    vi.mocked(invoke).mockResolvedValueOnce(json);
    await expect(loadPreferences()).resolves.toEqual({
      theme: 'light',
      recentWorkspacePaths: ['/a'],
    });
    expect(invoke).toHaveBeenCalledWith('read_app_preferences');
  });

  it('savePreferences invokes with serialized payload', async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await savePreferences(DEFAULT_APP_PREFERENCES);
    expect(invoke).toHaveBeenCalledWith('save_app_preferences', {
      json: expect.stringContaining('"recentWorkspacePaths"'),
    });
  });
});
