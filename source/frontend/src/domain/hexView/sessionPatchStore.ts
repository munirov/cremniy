/**
 * Session-wide patch history (Qt parity: PatchesTab + dialogs/patches).
 *
 * The HEX command stack lives inside BinaryToolPanel, but a separate Patches
 * tab needs to observe the same list across tab mounts. We mirror every
 * command into this module-level store so the Patches tab can subscribe
 * without sharing React context with the hex viewer.
 */

import {
  emptyHexCommandStack,
  pushCommand,
  type HexCommand,
  type HexCommandStackState,
} from './hexCommandStack';

export type SessionPatchEntry = HexCommand & {
  /** Monotonic id assigned on push — handy for keys and Revert lookups. */
  id: number;
  /** Workspace-relative file path the patch was applied to. */
  filePath: string;
};

let state: HexCommandStackState = emptyHexCommandStack();
let nextId = 1;
const entriesByFile = new Map<string, SessionPatchEntry[]>();
const listeners = new Set<(entries: readonly SessionPatchEntry[]) => void>();

export function pushSessionPatch(filePath: string, cmd: HexCommand): SessionPatchEntry {
  state = pushCommand(state, cmd);
  const id = nextId;
  nextId += 1;
  const entry: SessionPatchEntry = { ...cmd, id, filePath };
  const list = entriesByFile.get(filePath) ?? [];
  list.push(entry);
  entriesByFile.set(filePath, list);
  notify(filePath);
  return entry;
}

export function clearSessionPatches(filePath: string): void {
  entriesByFile.delete(filePath);
  state = emptyHexCommandStack();
  notify(filePath);
}

export function subscribeSessionPatches(
  filePath: string,
  listener: (entries: readonly SessionPatchEntry[]) => void,
): () => void {
  const fn: (entries: readonly SessionPatchEntry[]) => void = (entries) => listener(entries);
  listeners.add(fn);
  listener(entriesByFile.get(filePath) ?? []);
  return () => {
    listeners.delete(fn);
  };
}

function notify(filePath: string): void {
  const snapshot = entriesByFile.get(filePath) ?? [];
  for (const l of listeners) {
    l(snapshot);
  }
}
