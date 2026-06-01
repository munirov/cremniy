// Hex patching from the disassembly listing (Qt parity: "Patch bytes…").
// In Qt this was radare2-only because only that backend exposed a file offset;
// objdump section headers give us the same offset, so patching works with the
// default backend too. Patch = overwrite N bytes at an instruction's file offset.

export type HexPatchResult =
  | { ok: true; bytes: Uint8Array }
  | { ok: false; message: string };

/**
 * Overwrite `patch` bytes at `fileOffset` in a copy of `fileBytes`.
 * Rejects a null/negative offset or a patch that runs past end of file.
 */
export function applyHexPatchToFile(
  fileBytes: Uint8Array,
  fileOffset: number | null,
  patch: Uint8Array,
): HexPatchResult {
  if (fileOffset == null || !Number.isInteger(fileOffset) || fileOffset < 0) {
    return { ok: false, message: 'This instruction has no file offset to patch.' };
  }
  if (patch.length === 0) {
    return { ok: false, message: 'Enter at least one hex byte to write.' };
  }
  if (fileOffset + patch.length > fileBytes.length) {
    return { ok: false, message: 'Patch runs past the end of the file.' };
  }
  const next = new Uint8Array(fileBytes);
  next.set(patch, fileOffset);
  return { ok: true, bytes: next };
}
