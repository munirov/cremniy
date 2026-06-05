/**
 * Command-pattern history for hex-buffer edits (Qt parity: QHexDocument +
 * QUndoStack). Each user-visible edit produces a HexCommand that can be applied
 * and reverted; the stack keeps the last `capacity` commands and supports
 * undo/redo without losing the bytes between them.
 */

import { replaceRange } from './hexBufferSearch';

export type HexCommand =
  | { kind: 'insert'; offset: number; bytes: Uint8Array }
  | { kind: 'remove'; offset: number; removed: Uint8Array }
  | { kind: 'replace'; offset: number; oldBytes: Uint8Array; newBytes: Uint8Array };

export type HexCommandStackState = {
  undo: HexCommand[];
  redo: HexCommand[];
};

export const DEFAULT_HEX_STACK_CAPACITY = 500;

export function emptyHexCommandStack(): HexCommandStackState {
  return { undo: [], redo: [] };
}

export function applyCommand(buffer: Uint8Array, cmd: HexCommand): Uint8Array {
  switch (cmd.kind) {
    case 'insert':
      return replaceRange(buffer, cmd.offset, 0, cmd.bytes);
    case 'remove':
      return replaceRange(buffer, cmd.offset, cmd.removed.length, new Uint8Array(0));
    case 'replace':
      return replaceRange(buffer, cmd.offset, cmd.oldBytes.length, cmd.newBytes);
  }
}

export function revertCommand(buffer: Uint8Array, cmd: HexCommand): Uint8Array {
  switch (cmd.kind) {
    case 'insert':
      return replaceRange(buffer, cmd.offset, cmd.bytes.length, new Uint8Array(0));
    case 'remove':
      return replaceRange(buffer, cmd.offset, 0, cmd.removed);
    case 'replace':
      return replaceRange(buffer, cmd.offset, cmd.newBytes.length, cmd.oldBytes);
  }
}

export function pushCommand(
  state: HexCommandStackState,
  cmd: HexCommand,
  capacity: number = DEFAULT_HEX_STACK_CAPACITY,
): HexCommandStackState {
  const undo = state.undo.concat(cmd);
  while (undo.length > capacity) {
    undo.shift();
  }
  return { undo, redo: [] };
}

export type UndoResult = {
  state: HexCommandStackState;
  buffer: Uint8Array;
  command: HexCommand | null;
};

export function undo(state: HexCommandStackState, buffer: Uint8Array): UndoResult {
  const cmd = state.undo[state.undo.length - 1];
  if (cmd == null) {
    return { state, buffer, command: null };
  }
  return {
    state: {
      undo: state.undo.slice(0, -1),
      redo: state.redo.concat(cmd),
    },
    buffer: revertCommand(buffer, cmd),
    command: cmd,
  };
}

export function redo(state: HexCommandStackState, buffer: Uint8Array): UndoResult {
  const cmd = state.redo[state.redo.length - 1];
  if (cmd == null) {
    return { state, buffer, command: null };
  }
  return {
    state: {
      undo: state.undo.concat(cmd),
      redo: state.redo.slice(0, -1),
    },
    buffer: applyCommand(buffer, cmd),
    command: cmd,
  };
}

/** Drop all history — call after a save so the buffer becomes the new baseline. */
export function clearHistory(_state: HexCommandStackState): HexCommandStackState {
  return emptyHexCommandStack();
}

export function canUndo(state: HexCommandStackState): boolean {
  return state.undo.length > 0;
}

export function canRedo(state: HexCommandStackState): boolean {
  return state.redo.length > 0;
}
