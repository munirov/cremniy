// Terminal command history, ported from the Qt TerminalWidget behaviour:
// load/save a newline-separated list, keep the last MAX_HISTORY_ENTRIES,
// and skip a command equal to the previous one.

export const MAX_HISTORY_ENTRIES = 100;

/** Parse stored history text into trimmed, non-empty command lines. */
export function parseTerminalHistory(text: string): string[] {
  return text
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line !== '');
}

/** Serialize history for storage (one command per line, trailing newline). */
export function serializeTerminalHistory(history: readonly string[]): string {
  const capped = history.slice(Math.max(0, history.length - MAX_HISTORY_ENTRIES));
  return capped.length === 0 ? '' : `${capped.join('\n')}\n`;
}

/** Append a command, skipping consecutive duplicates and capping length. */
export function appendHistoryEntry(history: readonly string[], command: string): string[] {
  const trimmed = command.trim();
  if (trimmed === '' || history[history.length - 1] === trimmed) {
    return [...history];
  }
  const next = [...history, trimmed];
  return next.slice(Math.max(0, next.length - MAX_HISTORY_ENTRIES));
}
