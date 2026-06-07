export type TerminalMenuActionId = 'newTerminal' | 'openConnections';

export type TerminalMenuEntry = { id: TerminalMenuActionId; label: string };

export const TERMINAL_MENU_ENTRIES: readonly TerminalMenuEntry[] = [
  { id: 'newTerminal', label: 'New Terminal' },
  { id: 'openConnections', label: 'Connections (Hosts)…' },
] as const;
