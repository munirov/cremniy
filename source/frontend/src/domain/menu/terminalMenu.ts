export type TerminalMenuActionId = 'newTerminal';

export type TerminalMenuEntry = { id: TerminalMenuActionId; label: string };

export const TERMINAL_MENU_ENTRIES: readonly TerminalMenuEntry[] = [
  { id: 'newTerminal', label: 'New Terminal' },
] as const;
