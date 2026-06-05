/** Qt MenuFactory registrations: top-level menu keys "1".."7". */
export const MAIN_MENU_IDS = ['1', '2', '3', '4', '5', '6', '7'] as const;

export type MainMenuId = (typeof MAIN_MENU_IDS)[number];

export const MAIN_MENU_LABELS = ['File', 'Edit', 'View', 'Build', 'Tools', 'References', 'Terminal'] as const;

// i18n keys parallel to MAIN_MENU_LABELS — MenuBar runs each through t() so
// the menu strip switches language when prefs.locale flips.
export const MAIN_MENU_I18N_KEYS = [
  'menu.file',
  'menu.edit',
  'menu.view',
  'menu.build',
  'menu.tools',
  'menu.references',
  'menu.terminal',
] as const;

export type MainMenuEntry = { id: MainMenuId; label: string; i18nKey: string };

if (MAIN_MENU_IDS.length !== MAIN_MENU_LABELS.length) {
  throw new Error('MAIN_MENU_IDS and MAIN_MENU_LABELS must have the same length');
}

export function mainMenuEntries(): readonly MainMenuEntry[] {
  return MAIN_MENU_IDS.map((id, i) => ({
    id,
    label: MAIN_MENU_LABELS[i] as string,
    i18nKey: MAIN_MENU_I18N_KEYS[i] as string,
  }));
}

/** Implemented accelerators for the React shell (see `matchGlobalShortcut`). */
export const MENU_SHORTCUT_CONTRACT = [
  { menu: 'File', actionId: 'openFile', accelerator: 'Ctrl+O / Cmd+O' },
  { menu: 'File', actionId: 'openFolder', accelerator: 'Ctrl+Shift+O / Cmd+Shift+O' },
  { menu: 'File', actionId: 'save', accelerator: 'Ctrl+S / Cmd+S' },
  { menu: 'File', actionId: 'saveAs', accelerator: 'Ctrl+Shift+S / Cmd+Shift+S' },
  { menu: 'File', actionId: 'preferences', accelerator: 'Ctrl+, / Cmd+,' },
  { menu: 'File', actionId: 'closeEditorTab', accelerator: 'Ctrl+W / Cmd+W' },
  { menu: 'File', actionId: 'closeWorkspace', accelerator: 'Ctrl+Shift+W / Cmd+Shift+W' },
  { menu: 'Edit', actionId: 'findInEditor', accelerator: 'Ctrl+F / Cmd+F' },
  { menu: 'View', actionId: 'toggleTerminal', accelerator: 'Ctrl+` / Cmd+`' },
] as const;
