/** Qt MenuFactory registrations: top-level menu keys "1".."6". */
export const MAIN_MENU_IDS = ['1', '2', '3', '4', '5', '6'] as const;

export type MainMenuId = (typeof MAIN_MENU_IDS)[number];

export const MAIN_MENU_LABELS = ['File', 'Edit', 'View', 'Build', 'Tools', 'References'] as const;

export type MainMenuEntry = { id: MainMenuId; label: string };

if (MAIN_MENU_IDS.length !== MAIN_MENU_LABELS.length) {
  throw new Error('MAIN_MENU_IDS and MAIN_MENU_LABELS must have the same length');
}

export function mainMenuEntries(): readonly MainMenuEntry[] {
  return MAIN_MENU_IDS.map((id, i) => ({
    id,
    label: MAIN_MENU_LABELS[i] as string,
  }));
}
