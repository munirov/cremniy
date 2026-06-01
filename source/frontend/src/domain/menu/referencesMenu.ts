export type ReferencesMenuActionId = 'asciiTable' | 'scancodeTable';

export type ReferencesMenuEntry = { id: ReferencesMenuActionId; label: string };

export const REFERENCES_MENU_ENTRIES: readonly ReferencesMenuEntry[] = [
  { id: 'asciiTable', label: 'ASCII chart' },
  { id: 'scancodeTable', label: 'Scancodes' },
] as const;
