import {
  parseAppPreferences,
  stringifyAppPreferences,
} from '@domain/preferences/appPreferences';
import type { SettingsService } from '@domain/preferences/settingsService';
import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import {
  pickFile,
  pickSaveFile,
  readUserFile,
  testObjdumpTool,
  writeUserFile,
} from '@infrastructure/tauri/bridge';

const PREFERENCES_EXPORT_FILE = 'cremniy-settings.json';

export const settingsService: SettingsService = {
  loadPreferences,
  savePreferences,
  testObjdumpTool,

  async exportPreferences() {
    const prefs = await loadPreferences();
    const target = await pickSaveFile(PREFERENCES_EXPORT_FILE);
    if (target == null || target === '') {
      return null;
    }
    await writeUserFile(target, stringifyAppPreferences(prefs));
    return target;
  },

  async importPreferences() {
    const source = await pickFile();
    if (source == null || source === '') {
      return null;
    }
    const text = await readUserFile(source);
    // parseAppPreferences normalizes/validates and falls back on bad input.
    const prefs = parseAppPreferences(text);
    await savePreferences(prefs);
    return prefs;
  },
};
