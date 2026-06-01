import { describe, expect, it } from 'vitest';

import {
  createBinaryBufferState,
  replaceBinaryBufferByte,
  replaceBinaryBufferBytes,
  resetBinaryBuffer,
  setBinaryBufferSelection,
  snapshotBinaryBufferBytes,
  snapshotBinaryBufferOriginalBytes,
  snapshotBinaryBufferSelection,
} from './binaryBuffer';

function bytesOf(stateBytes: Uint8Array): number[] {
  return Array.from(stateBytes);
}

function currentBytesOf(state: ReturnType<typeof createBinaryBufferState>): number[] {
  return bytesOf(snapshotBinaryBufferBytes(state));
}

describe('binaryBuffer', () => {
  it('creates an immutable state snapshot from caller bytes', () => {
    const input = new Uint8Array([0x41, 0x42]);
    const state = createBinaryBufferState(input);

    input[0] = 0xff;

    expect(bytesOf(snapshotBinaryBufferOriginalBytes(state))).toEqual([0x41, 0x42]);
    expect(currentBytesOf(state)).toEqual([0x41, 0x42]);
    expect(state.overlays).toEqual([]);
    expect(state.isDirty).toBe(false);
    expect(snapshotBinaryBufferSelection(state)).toBeNull();
  });

  it('replaces one byte without mutating the previous state', () => {
    const state = createBinaryBufferState(new Uint8Array([0x41, 0x42, 0x43]));
    const updated = replaceBinaryBufferByte(state, 1, 0xff);

    expect(currentBytesOf(state)).toEqual([0x41, 0x42, 0x43]);
    expect(currentBytesOf(updated)).toEqual([0x41, 0xff, 0x43]);
    expect(updated.overlays).toEqual([{ offset: 1, value: 0xff }]);
    expect(updated.isDirty).toBe(true);
  });

  it('returns byte snapshots that cannot mutate state', () => {
    const state = createBinaryBufferState(new Uint8Array([0x01, 0x02]));
    const updated = replaceBinaryBufferByte(state, 1, 0xff);
    const stateOriginal = snapshotBinaryBufferOriginalBytes(state);
    const stateCurrent = snapshotBinaryBufferBytes(state);
    const updatedOriginal = snapshotBinaryBufferOriginalBytes(updated);
    const updatedCurrent = snapshotBinaryBufferBytes(updated);

    stateOriginal[1] = 0x00;
    stateCurrent[1] = 0x00;
    updatedOriginal[1] = 0x00;
    updatedCurrent[1] = 0x00;

    expect(bytesOf(snapshotBinaryBufferOriginalBytes(state))).toEqual([0x01, 0x02]);
    expect(currentBytesOf(state)).toEqual([0x01, 0x02]);
    expect(bytesOf(snapshotBinaryBufferOriginalBytes(updated))).toEqual([0x01, 0x02]);
    expect(currentBytesOf(updated)).toEqual([0x01, 0xff]);
    expect(updated.overlays).toEqual([{ offset: 1, value: 0xff }]);
  });

  it('replaces multiple bytes and tracks changed overlays only', () => {
    const state = createBinaryBufferState(new Uint8Array([0x10, 0x20, 0x30, 0x40]));
    const updated = replaceBinaryBufferBytes(state, 1, new Uint8Array([0xaa, 0x30]));

    expect(currentBytesOf(updated)).toEqual([0x10, 0xaa, 0x30, 0x40]);
    expect(updated.overlays).toEqual([{ offset: 1, value: 0xaa }]);
    expect(updated.isDirty).toBe(true);
  });

  it('replaces multiple bytes at buffer boundaries', () => {
    const state = createBinaryBufferState(new Uint8Array([0x10, 0x20, 0x30, 0x40]));
    const replacedStart = replaceBinaryBufferBytes(state, 0, new Uint8Array([0xaa, 0xbb]));
    const replacedEnd = replaceBinaryBufferBytes(
      replacedStart,
      2,
      new Uint8Array([0xcc, 0xdd]),
    );

    expect(currentBytesOf(replacedStart)).toEqual([0xaa, 0xbb, 0x30, 0x40]);
    expect(replacedStart.overlays).toEqual([
      { offset: 0, value: 0xaa },
      { offset: 1, value: 0xbb },
    ]);
    expect(currentBytesOf(replacedEnd)).toEqual([0xaa, 0xbb, 0xcc, 0xdd]);
    expect(replacedEnd.overlays).toEqual([
      { offset: 0, value: 0xaa },
      { offset: 1, value: 0xbb },
      { offset: 2, value: 0xcc },
      { offset: 3, value: 0xdd },
    ]);
  });

  it('does not retain caller-owned replacement bytes', () => {
    const state = createBinaryBufferState(new Uint8Array([0x01, 0x02, 0x03]));
    const replacement = new Uint8Array([0xaa, 0xbb]);
    const updated = replaceBinaryBufferBytes(state, 1, replacement);

    replacement[0] = 0x00;
    replacement[1] = 0x00;

    expect(currentBytesOf(updated)).toEqual([0x01, 0xaa, 0xbb]);
    expect(updated.overlays).toEqual([
      { offset: 1, value: 0xaa },
      { offset: 2, value: 0xbb },
    ]);
  });

  it('clears dirty state when a replacement restores original bytes', () => {
    const state = createBinaryBufferState(new Uint8Array([0x01, 0x02]));
    const changed = replaceBinaryBufferByte(state, 0, 0xff);
    const restored = replaceBinaryBufferByte(changed, 0, 0x01);

    expect(currentBytesOf(restored)).toEqual([0x01, 0x02]);
    expect(restored.overlays).toEqual([]);
    expect(restored.isDirty).toBe(false);
  });

  it('resets current bytes and dirty overlays while preserving selection', () => {
    const state = createBinaryBufferState(new Uint8Array([0x01, 0x02, 0x03]));
    const selected = setBinaryBufferSelection(state, { start: 1, endExclusive: 3 });
    const changed = replaceBinaryBufferByte(selected, 1, 0xff);
    const reset = resetBinaryBuffer(changed);

    expect(currentBytesOf(reset)).toEqual([0x01, 0x02, 0x03]);
    expect(reset.overlays).toEqual([]);
    expect(reset.isDirty).toBe(false);
    expect(snapshotBinaryBufferSelection(reset)).toEqual({ start: 1, endExclusive: 3 });
  });

  it('rejects invalid replace offsets', () => {
    const state = createBinaryBufferState(new Uint8Array([0x01]));

    expect(() => replaceBinaryBufferByte(state, -1, 0x00)).toThrow(RangeError);
    expect(() => replaceBinaryBufferByte(state, 1, 0x00)).toThrow(RangeError);
    expect(() => replaceBinaryBufferBytes(state, 1, new Uint8Array([0x00]))).toThrow(RangeError);
    expect(() => replaceBinaryBufferBytes(state, 0, new Uint8Array([0x00, 0x01]))).toThrow(
      RangeError,
    );
  });

  it('rejects invalid byte values', () => {
    const state = createBinaryBufferState(new Uint8Array([0x01]));

    expect(() => replaceBinaryBufferByte(state, 0, -1)).toThrow(RangeError);
    expect(() => replaceBinaryBufferByte(state, 0, 256)).toThrow(RangeError);
    expect(() => replaceBinaryBufferByte(state, 0, 1.5)).toThrow(RangeError);
  });

  it('sets, clears, and validates an active selection', () => {
    const state = createBinaryBufferState(new Uint8Array([0x01, 0x02, 0x03]));
    const selected = setBinaryBufferSelection(state, { start: 0, endExclusive: 2 });
    const cleared = setBinaryBufferSelection(selected, { start: 1, endExclusive: 1 });

    expect(snapshotBinaryBufferSelection(selected)).toEqual({ start: 0, endExclusive: 2 });
    expect(snapshotBinaryBufferSelection(cleared)).toBeNull();
    expect(() => setBinaryBufferSelection(state, { start: 2, endExclusive: 1 })).toThrow(
      RangeError,
    );
    expect(() => setBinaryBufferSelection(state, { start: 0, endExclusive: 4 })).toThrow(
      RangeError,
    );
  });

  it('snapshots selection so caller mutations cannot corrupt state', () => {
    const selection = { start: 0, endExclusive: 2 };
    const selected = createBinaryBufferState(new Uint8Array([0x01, 0x02, 0x03]), selection);
    const selectedSnapshot = snapshotBinaryBufferSelection(selected);

    selection.start = 1;
    if (selectedSnapshot != null) {
      (selectedSnapshot as { start: number }).start = 1;
    }

    expect(snapshotBinaryBufferSelection(selected)).toEqual({ start: 0, endExclusive: 2 });
  });

  it('returns cloned snapshots for save operations', () => {
    const state = replaceBinaryBufferByte(
      createBinaryBufferState(new Uint8Array([0x01, 0x02])),
      1,
      0xff,
    );

    const snapshot = snapshotBinaryBufferBytes(state);
    snapshot[1] = 0x00;

    expect(currentBytesOf(state)).toEqual([0x01, 0xff]);
    expect(state.overlays).toEqual([{ offset: 1, value: 0xff }]);
    expect(state.isDirty).toBe(true);
  });
});
