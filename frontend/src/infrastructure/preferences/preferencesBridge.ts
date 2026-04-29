import { invoke } from '@tauri-apps/api/core';

import {
  parseAppPreferences,
  stringifyAppPreferences,
  type AppPreferences,
} from '@domain/preferences/appPreferences';

export async function loadPreferences(): Promise<AppPreferences> {
  const json = await invoke<string>('read_app_preferences');
  return parseAppPreferences(json);
}

export async function savePreferences(prefs: AppPreferences): Promise<void> {
  await invoke<void>('save_app_preferences', {
    json: stringifyAppPreferences(prefs),
  });
}
