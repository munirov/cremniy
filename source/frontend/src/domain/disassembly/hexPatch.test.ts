import { describe, expect, it } from 'vitest';

import { applyHexPatchToFile } from './hexPatch';

describe('applyHexPatchToFile', () => {
  it('overwrites bytes at the file offset without touching the rest', () => {
    const file = new Uint8Array([0x00, 0x11, 0x22, 0x33, 0x44]);
    const result = applyHexPatchToFile(file, 1, new Uint8Array([0xaa, 0xbb]));
    expect(result.ok).toBe(true);
    if (result.ok) {
      expect(Array.from(result.bytes)).toEqual([0x00, 0xaa, 0xbb, 0x33, 0x44]);
    }
    // original is untouched (pure)
    expect(Array.from(file)).toEqual([0x00, 0x11, 0x22, 0x33, 0x44]);
  });

  it('rejects a null/invalid offset', () => {
    const file = new Uint8Array([0x00, 0x11]);
    expect(applyHexPatchToFile(file, null, new Uint8Array([0x01]))).toEqual({
      ok: false,
      message: 'This instruction has no file offset to patch.',
    });
    expect(applyHexPatchToFile(file, -1, new Uint8Array([0x01])).ok).toBe(false);
  });

  it('rejects an empty patch', () => {
    const file = new Uint8Array([0x00, 0x11]);
    expect(applyHexPatchToFile(file, 0, new Uint8Array([])).ok).toBe(false);
  });

  it('rejects a patch that runs past end of file', () => {
    const file = new Uint8Array([0x00, 0x11]);
    expect(applyHexPatchToFile(file, 1, new Uint8Array([0xaa, 0xbb]))).toEqual({
      ok: false,
      message: 'Patch runs past the end of the file.',
    });
  });

  it('allows a patch that exactly reaches end of file', () => {
    const file = new Uint8Array([0x00, 0x11]);
    const result = applyHexPatchToFile(file, 0, new Uint8Array([0xaa, 0xbb]));
    expect(result.ok).toBe(true);
  });
});
