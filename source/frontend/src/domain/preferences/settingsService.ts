import type { AppPreferences } from './appPreferences';

export type SettingsService = {
  loadPreferences: () => Promise<AppPreferences>;
  savePreferences: (prefs: AppPreferences) => Promise<void>;
  testObjdumpTool: (workspaceRoot?: string | null, objdumpPath?: string | null) => Promise<string>;
  /** Write current preferences to a user-chosen file. Returns the path, or null if cancelled. */
  exportPreferences: () => Promise<string | null>;
  /** Read preferences from a user-chosen file and persist them. Returns the imported set, or null if cancelled. */
  importPreferences: () => Promise<AppPreferences | null>;
};
