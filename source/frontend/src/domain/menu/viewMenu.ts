export type ViewMenuActionId = 'toggleTerminal' | 'toggleWordWrap';

export type ViewMenuEntry = { id: ViewMenuActionId; label: string };

export const VIEW_MENU_ENTRIES: readonly ViewMenuEntry[] = [
  { id: 'toggleTerminal', label: 'Terminal panel' },
  { id: 'toggleWordWrap', label: 'Word wrap' },
] as const;
