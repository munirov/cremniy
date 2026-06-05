import {
  parseAppPreferencesIni,
  stringifyAppPreferencesIni,
} from '@domain/preferences/iniSerialization';
import type { SettingsService } from '@domain/preferences/settingsService';
import { loadPreferences, savePreferences } from '@infrastructure/preferences/preferencesBridge';
import {
  pickFile,
  pickSaveFile,
  readUserFile,
  testObjdumpTool,
  writeUserFile,
} from '@infrastructure/tauri/bridge';

// Qt parity — humans expect a .ini file from File → Export Settings.
const PREFERENCES_EXPORT_FILE = 'cremniy-settings.ini';

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
    await writeUserFile(target, stringifyAppPreferencesIni(prefs));
    return target;
  },

  async importPreferences() {
    const source = await pickFile();
    if (source == null || source === '') {
      return null;
    }
    const text = await readUserFile(source);
    const prefs = parseAppPreferencesIni(text);
    await savePreferences(prefs);
    return prefs;
  },
};
