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
      locale: DEFAULT_APP_PREFERENCES.locale,
      recentWorkspacePaths: ['/a'],
      terminalPanelVisible: true,
      editorWordWrap: DEFAULT_APP_PREFERENCES.editorWordWrap,
      editorFontSize: DEFAULT_APP_PREFERENCES.editorFontSize,
      editorInsertSpaces: DEFAULT_APP_PREFERENCES.editorInsertSpaces,
      editorTabWidth: DEFAULT_APP_PREFERENCES.editorTabWidth,
      excludedFilePatterns: DEFAULT_APP_PREFERENCES.excludedFilePatterns,
      hexOptions: DEFAULT_APP_PREFERENCES.hexOptions,
      disassembly: DEFAULT_APP_PREFERENCES.disassembly,
      dockLayout: DEFAULT_APP_PREFERENCES.dockLayout,
    });
    expect(invoke).toHaveBeenCalledWith('read_app_preferences');
  });

  it('savePreferences invokes with serialized payload', async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await savePreferences(DEFAULT_APP_PREFERENCES);
    expect(invoke).toHaveBeenCalledWith('save_app_preferences', {
      json: expect.stringContaining('"terminalPanelVisible"'),
    });
  });
});
