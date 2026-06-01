import { describe, expect, it } from 'vitest';

import {
  findAllSubsequenceIndices,
  parseHexByteSequence,
  parseOffsetInput,
} from './hexBufferSearch';

describe('findAllSubsequenceIndices', () => {
  it('returns empty for empty needle', () => {
    expect(findAllSubsequenceIndices(new Uint8Array([1, 2, 3]), new Uint8Array())).toEqual([]);
  });

  it('returns empty when needle is longer than haystack', () => {
    expect(findAllSubsequenceIndices(new Uint8Array([1]), new Uint8Array([1, 2]))).toEqual([]);
  });

  it('finds single occurrence', () => {
    const h = new Uint8Array([0x41, 0x42, 0x43]);
    expect(findAllSubsequenceIndices(h, new Uint8Array([0x42, 0x43]))).toEqual([1]);
  });

  it('finds multiple occurrences', () => {
    const h = new Uint8Array([0xaa, 0x01, 0xaa, 0x01]);
    expect(findAllSubsequenceIndices(h, new Uint8Array([0xaa, 0x01]))).toEqual([0, 2]);
  });

  it('finds overlapping matches', () => {
    const h = new Uint8Array([0xff, 0xff, 0xff]);
    expect(findAllSubsequenceIndices(h, new Uint8Array([0xff, 0xff]))).toEqual([0, 1]);
  });

  it('returns empty when there is no match', () => {
    expect(findAllSubsequenceIndices(new Uint8Array([1, 2, 3]), new Uint8Array([9]))).toEqual([]);
  });

  it('matches full buffer', () => {
    const h = new Uint8Array([9]);
    expect(findAllSubsequenceIndices(h, new Uint8Array([9]))).toEqual([0]);
  });
});

describe('parseOffsetInput', () => {
  it('rejects empty offset', () => {
    expect(parseOffsetInput('', 'decimal')).toEqual({ ok: false, message: 'Enter an offset.' });
    expect(parseOffsetInput('   ', 'hex')).toEqual({ ok: false, message: 'Enter an offset.' });
  });

  it('parses decimal', () => {
    expect(parseOffsetInput('1024', 'decimal')).toEqual({ ok: true, value: 1024 });
    expect(parseOffsetInput('0', 'decimal')).toEqual({ ok: true, value: 0 });
  });

  it('rejects decimal offset that is not a safe integer', () => {
    const r = parseOffsetInput('9007199254740992', 'decimal');
    expect(r.ok).toBe(false);
    if (!r.ok) {
      expect(r.message).toBe('Offset is too large.');
    }
  });

  it('rejects invalid decimal', () => {
    expect(parseOffsetInput('10a', 'decimal')).toEqual({
      ok: false,
      message: 'Decimal offset must be non-negative digits only.',
    });
    expect(parseOffsetInput('-1', 'decimal').ok).toBe(false);
  });

  it('parses hex with optional 0x and spaces', () => {
    expect(parseOffsetInput('ff', 'hex')).toEqual({ ok: true, value: 255 });
    expect(parseOffsetInput('0x10', 'hex')).toEqual({ ok: true, value: 16 });
    expect(parseOffsetInput('0Xdead', 'hex')).toEqual({ ok: true, value: 0xdead });
    expect(parseOffsetInput('00 00 10', 'hex')).toEqual({ ok: true, value: 16 });
  });

  it('rejects invalid hex', () => {
    expect(parseOffsetInput('0x', 'hex')).toEqual({ ok: false, message: 'Enter a hex offset.' });
    expect(parseOffsetInput('gg', 'hex')).toEqual({
      ok: false,
      message: 'Hex offset must contain hex digits only.',
    });
  });

  it('rejects hex offset above Number.MAX_SAFE_INTEGER', () => {
    const r = parseOffsetInput('20000000000000', 'hex');
    expect(r.ok).toBe(false);
    if (!r.ok) {
      expect(r.message).toBe('Offset is too large.');
    }
  });
});

describe('parseHexByteSequence', () => {
  it('parses continuous hex', () => {
    const r = parseHexByteSequence('4142');
    expect(r.ok).toBe(true);
    if (r.ok) {
      expect([...r.bytes]).toEqual([0x41, 0x42]);
    }
  });

  it('parses space-separated pairs', () => {
    const r = parseHexByteSequence('41 42  43');
    expect(r.ok).toBe(true);
    if (r.ok) {
      expect([...r.bytes]).toEqual([0x41, 0x42, 0x43]);
    }
  });

  it('strips non-hex noise', () => {
    const r = parseHexByteSequence('0x41, 0x42');
    expect(r.ok).toBe(true);
    if (r.ok) {
      expect([...r.bytes]).toEqual([0x41, 0x42]);
    }
  });

  it('rejects odd digit count', () => {
    expect(parseHexByteSequence('414')).toEqual({
      ok: false,
      message: 'Hex bytes need an even number of digits.',
    });
  });

  it('rejects empty meaningful input', () => {
    expect(parseHexByteSequence('   ')).toEqual({
      ok: false,
      message: 'Enter hex bytes (e.g. 41 42 or 4142).',
    });
    expect(parseHexByteSequence('zzz')).toEqual({
      ok: false,
      message: 'Enter hex bytes (e.g. 41 42 or 4142).',
    });
  });
});
