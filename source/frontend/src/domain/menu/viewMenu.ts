export type ViewMenuActionId =
  | 'toggleTerminal'
  | 'toggleWordWrap'
  | 'toggleFileTree'
  | 'toggleInsertSpaces'
  | 'setTabWidth2'
  | 'setTabWidth4'
  | 'setTabWidth8';

export type ViewMenuEntry = { id: ViewMenuActionId; label: string };

export const VIEW_MENU_ENTRIES: readonly ViewMenuEntry[] = [
  { id: 'toggleFileTree', label: 'File tree' },
  { id: 'toggleTerminal', label: 'Terminal panel' },
  { id: 'toggleWordWrap', label: 'Word wrap' },
  { id: 'toggleInsertSpaces', label: 'Insert spaces instead of tab' },
  { id: 'setTabWidth2', label: 'Tab width: 2' },
  { id: 'setTabWidth4', label: 'Tab width: 4' },
  { id: 'setTabWidth8', label: 'Tab width: 8' },
] as const;
