// Persists terminal command history under the app config dir
// (Qt parity: terminal_history.txt). Load/save are best-effort: a missing
// file or IO error yields an empty history rather than surfacing to the user.

import {
  parseTerminalHistory,
  serializeTerminalHistory,
} from '@domain/terminal/terminalHistory';
import { readTextFile, writeAppConfig } from '@infrastructure/tauri/bridge';

const HISTORY_FILE = 'terminal_history.txt';

export async function loadTerminalHistory(): Promise<string[]> {
  try {
    const text = await readTextFile(HISTORY_FILE);
    return parseTerminalHistory(text);
  } catch {
    return [];
  }
}

export async function saveTerminalHistory(history: readonly string[]): Promise<void> {
  try {
    await writeAppConfig(HISTORY_FILE, serializeTerminalHistory(history));
  } catch {
    // Non-fatal: history persistence is a convenience, not correctness.
  }
}
