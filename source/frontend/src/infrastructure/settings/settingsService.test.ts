import { describe, expect, it, vi } from 'vitest';

const preferencesBridge = vi.hoisted(() => ({
  loadPreferences: vi.fn(),
  savePreferences: vi.fn(),
}));

const tauriBridge = vi.hoisted(() => ({
  testObjdumpTool: vi.fn(),
}));

vi.mock('@infrastructure/preferences/preferencesBridge', () => ({
  loadPreferences: preferencesBridge.loadPreferences,
  savePreferences: preferencesBridge.savePreferences,
}));

vi.mock('@infrastructure/tauri/bridge', () => ({
  testObjdumpTool: tauriBridge.testObjdumpTool,
}));

import { DEFAULT_APP_PREFERENCES } from '@domain/preferences/appPreferences';

import { settingsService } from './settingsService';

describe('settingsService', () => {
  it('adapts preference and tool bridge functions to the domain service contract', async () => {
    preferencesBridge.loadPreferences.mockResolvedValue(DEFAULT_APP_PREFERENCES);
    preferencesBridge.savePreferences.mockResolvedValue(undefined);
    tauriBridge.testObjdumpTool.mockResolvedValue('objdump OK');

    await expect(settingsService.loadPreferences()).resolves.toBe(DEFAULT_APP_PREFERENCES);
    await expect(settingsService.savePreferences(DEFAULT_APP_PREFERENCES)).resolves.toBeUndefined();
    await expect(settingsService.testObjdumpTool('/workspace', '/usr/bin/objdump')).resolves.toBe(
      'objdump OK',
    );

    expect(preferencesBridge.loadPreferences).toHaveBeenCalledTimes(1);
    expect(preferencesBridge.savePreferences).toHaveBeenCalledWith(DEFAULT_APP_PREFERENCES);
    expect(tauriBridge.testObjdumpTool).toHaveBeenCalledWith('/workspace', '/usr/bin/objdump');
  });
});
