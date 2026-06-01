export type BinaryBufferSelection = Readonly<{
  start: number;
  endExclusive: number;
}>;

export type BinaryByteOverlay = Readonly<{
  offset: number;
  value: number;
}>;

export class BinaryBufferState {
  readonly #originalBytes: Uint8Array;
  readonly #currentBytes: Uint8Array;
  readonly #selection: BinaryBufferSelection | null;

  readonly overlays: readonly BinaryByteOverlay[];
  readonly isDirty: boolean;

  private constructor(
    originalBytes: Uint8Array,
    currentBytes: Uint8Array,
    selection: BinaryBufferSelection | null,
  ) {
    this.#originalBytes = cloneBytes(originalBytes);
    this.#currentBytes = cloneBytes(currentBytes);
    this.#selection = cloneSelection(selection);
    this.overlays = Object.freeze(
      computeOverlays(this.#originalBytes, this.#currentBytes).map((overlay) =>
        Object.freeze(overlay),
      ),
    );
    this.isDirty = this.overlays.length > 0;
  }

  static fromSnapshots(
    originalBytes: Uint8Array,
    currentBytes: Uint8Array,
    selection: BinaryBufferSelection | null,
  ): BinaryBufferState {
    return new BinaryBufferState(originalBytes, currentBytes, selection);
  }

  snapshotOriginalBytes(): Uint8Array {
    return cloneBytes(this.#originalBytes);
  }

  snapshotCurrentBytes(): Uint8Array {
    return cloneBytes(this.#currentBytes);
  }

  snapshotSelection(): BinaryBufferSelection | null {
    return cloneSelection(this.#selection);
  }
}

function cloneBytes(bytes: Uint8Array): Uint8Array {
  return new Uint8Array(bytes);
}

function cloneSelection(selection: BinaryBufferSelection | null): BinaryBufferSelection | null {
  if (selection == null) {
    return null;
  }
  return { start: selection.start, endExclusive: selection.endExclusive };
}

function assertValidOffset(offset: number, length: number): void {
  if (!Number.isSafeInteger(offset) || offset < 0 || offset >= length) {
    throw new RangeError(`offset must be an integer between 0 and ${Math.max(0, length - 1)}`);
  }
}

function assertValidRangeStart(offset: number, length: number): void {
  if (!Number.isSafeInteger(offset) || offset < 0 || offset > length) {
    throw new RangeError(`offset must be an integer between 0 and ${length}`);
  }
}

function assertValidByte(value: number): void {
  if (!Number.isInteger(value) || value < 0 || value > 0xff) {
    throw new RangeError('byte value must be an integer between 0 and 255');
  }
}

function assertValidSelection(selection: BinaryBufferSelection, length: number): void {
  if (
    !Number.isSafeInteger(selection.start) ||
    !Number.isSafeInteger(selection.endExclusive) ||
    selection.start < 0 ||
    selection.endExclusive < selection.start ||
    selection.endExclusive > length
  ) {
    throw new RangeError('selection must be a valid half-open byte range');
  }
}

function normalizeSelection(
  selection: BinaryBufferSelection | null | undefined,
  length: number,
): BinaryBufferSelection | null {
  if (selection == null) {
    return null;
  }
  assertValidSelection(selection, length);
  if (selection.start === selection.endExclusive) {
    return null;
  }
  return { start: selection.start, endExclusive: selection.endExclusive };
}

function computeOverlays(originalBytes: Uint8Array, currentBytes: Uint8Array): BinaryByteOverlay[] {
  const overlays: BinaryByteOverlay[] = [];
  for (let offset = 0; offset < currentBytes.length; offset += 1) {
    const value = currentBytes[offset]!;
    if (value !== originalBytes[offset]) {
      overlays.push({ offset, value });
    }
  }
  return overlays;
}

function buildState(
  originalBytes: Uint8Array,
  currentBytes: Uint8Array,
  selection: BinaryBufferSelection | null,
): BinaryBufferState {
  return BinaryBufferState.fromSnapshots(originalBytes, currentBytes, selection);
}

export function createBinaryBufferState(
  bytes: Uint8Array,
  selection?: BinaryBufferSelection | null,
): BinaryBufferState {
  const originalBytes = cloneBytes(bytes);
  const currentBytes = cloneBytes(bytes);
  return buildState(originalBytes, currentBytes, normalizeSelection(selection, bytes.length));
}

export function replaceBinaryBufferByte(
  state: BinaryBufferState,
  offset: number,
  value: number,
): BinaryBufferState {
  const currentBytes = state.snapshotCurrentBytes();
  assertValidOffset(offset, currentBytes.length);
  assertValidByte(value);

  currentBytes[offset] = value;
  return buildState(state.snapshotOriginalBytes(), currentBytes, state.snapshotSelection());
}

export function replaceBinaryBufferBytes(
  state: BinaryBufferState,
  offset: number,
  replacementBytes: Uint8Array,
): BinaryBufferState {
  const currentBytes = state.snapshotCurrentBytes();
  assertValidRangeStart(offset, currentBytes.length);
  const endExclusive = offset + replacementBytes.length;
  if (endExclusive > currentBytes.length) {
    throw new RangeError('replacement range exceeds buffer length');
  }

  for (const value of replacementBytes) {
    assertValidByte(value);
  }

  currentBytes.set(replacementBytes, offset);
  return buildState(state.snapshotOriginalBytes(), currentBytes, state.snapshotSelection());
}

export function resetBinaryBuffer(state: BinaryBufferState): BinaryBufferState {
  const originalBytes = state.snapshotOriginalBytes();
  return buildState(originalBytes, originalBytes, state.snapshotSelection());
}

export function setBinaryBufferSelection(
  state: BinaryBufferState,
  selection: BinaryBufferSelection | null,
): BinaryBufferState {
  const currentBytes = state.snapshotCurrentBytes();
  return buildState(
    state.snapshotOriginalBytes(),
    currentBytes,
    normalizeSelection(selection, currentBytes.length),
  );
}

export function snapshotBinaryBufferOriginalBytes(state: BinaryBufferState): Uint8Array {
  return state.snapshotOriginalBytes();
}

export function snapshotBinaryBufferBytes(state: BinaryBufferState): Uint8Array {
  return state.snapshotCurrentBytes();
}

export function snapshotBinaryBufferSelection(
  state: BinaryBufferState,
): BinaryBufferSelection | null {
  return state.snapshotSelection();
}
