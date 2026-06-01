import { describe, expect, it } from 'vitest';

import type { DisassemblySection } from './disassembly';
import {
  extractAsciiStrings,
  indexStringsByFileOffset,
  resolveStringComment,
  vaddrToFileOffset,
} from './stringRefs';

function section(overrides: Partial<DisassemblySection>): DisassemblySection {
  return {
    name: '.rodata',
    vaddr: 0x2000,
    fileOffset: 0x1000,
    size: 0x100,
    hasFileMapping: true,
    rows: [],
    ...overrides,
  };
}

function bytesOf(text: string, opts: { nul?: boolean } = {}): Uint8Array {
  const base = Array.from(text, (c) => c.charCodeAt(0));
  return new Uint8Array(opts.nul === false ? base : [...base, 0x00]);
}

describe('extractAsciiStrings', () => {
  it('extracts NUL-terminated printable runs of at least the min length', () => {
    const data = new Uint8Array([
      ...bytesOf('Hello'), // offsets 0-5 (5 chars + NUL), terminated
      0x01,
      0x02, // offsets 6,7 — non-printable
      ...bytesOf('Hi'), // offsets 8-10 (2 chars + NUL) — too short, dropped
      ...bytesOf('world'), // offsets 11-16, terminated
    ]);
    expect(extractAsciiStrings(data)).toEqual([
      { fileOffset: 0, value: 'Hello' },
      { fileOffset: 11, value: 'world' },
    ]);
  });

  it('ignores runs without a NUL terminator', () => {
    const data = bytesOf('tail', { nul: false });
    expect(extractAsciiStrings(data)).toEqual([]);
  });

  it('respects a custom minimum length', () => {
    const data = bytesOf('ok');
    expect(extractAsciiStrings(data, 2)).toEqual([{ fileOffset: 0, value: 'ok' }]);
  });
});

describe('vaddrToFileOffset', () => {
  it('maps a vaddr inside a mapped section', () => {
    expect(vaddrToFileOffset(0x2004, [section({})])).toBe(0x1004);
  });

  it('returns null outside any section and for unmapped sections', () => {
    expect(vaddrToFileOffset(0x9999, [section({})])).toBeNull();
    expect(vaddrToFileOffset(0x2004, [section({ hasFileMapping: false })])).toBeNull();
  });
});

describe('resolveStringComment', () => {
  it('renders a quoted comment for an operand address that maps to a string', () => {
    const strings = indexStringsByFileOffset([{ fileOffset: 0x1004, value: 'Hello world' }]);
    const comment = resolveStringComment('lea rax, [rip + 0x2004]', [section({})], strings);
    expect(comment).toBe('"Hello world"');
  });

  it('returns null when no address resolves to a string', () => {
    const strings = indexStringsByFileOffset([{ fileOffset: 0x1004, value: 'Hello' }]);
    expect(resolveStringComment('mov rax, rbx', [section({})], strings)).toBeNull();
    expect(resolveStringComment('lea rax, [rip + 0x9999]', [section({})], strings)).toBeNull();
  });

  it('escapes newlines and truncates very long strings', () => {
    const long = 'a'.repeat(200);
    const strings = indexStringsByFileOffset([{ fileOffset: 0x1004, value: long }]);
    const comment = resolveStringComment('mov rax, 0x2004', [section({})], strings);
    expect(comment).toMatch(/^"a{80}…"$/);
  });
});
