import { describe, expect, it } from 'vitest';

import {
  alignOffsetToRowStart,
  byteSpanForRows,
  byteToAsciiColumnChar,
  byteToHexPair,
  computeVisibleHexRows,
} from './hexViewModel';

describe('byteToHexPair', () => {
  it('formats zero and max byte', () => {
    expect(byteToHexPair(0)).toBe('00');
    expect(byteToHexPair(255)).toBe('ff');
  });

  it('masks to one byte', () => {
    expect(byteToHexPair(0x11f)).toBe('1f');
  });
});

describe('byteToAsciiColumnChar', () => {
  it('prints printable ASCII', () => {
    expect(byteToAsciiColumnChar(0x20)).toBe(' ');
    expect(byteToAsciiColumnChar(0x41)).toBe('A');
    expect(byteToAsciiColumnChar(0x7e)).toBe('~');
  });

  it('replaces non-printable with dot', () => {
    expect(byteToAsciiColumnChar(0)).toBe('.');
    expect(byteToAsciiColumnChar(0x0a)).toBe('.');
    expect(byteToAsciiColumnChar(0x7f)).toBe('.');
    expect(byteToAsciiColumnChar(0xff)).toBe('.');
  });
});

describe('alignOffsetToRowStart', () => {
  it('aligns down to row boundary', () => {
    expect(alignOffsetToRowStart(17, 16)).toBe(16);
    expect(alignOffsetToRowStart(16, 16)).toBe(16);
    expect(alignOffsetToRowStart(0, 8)).toBe(0);
    expect(alignOffsetToRowStart(7, 8)).toBe(0);
  });

  it('clamps aligned row start to zero when offset is negative', () => {
    expect(alignOffsetToRowStart(-10, 8)).toBe(0);
  });

  it('returns offset when bytesPerRow is non-positive', () => {
    expect(alignOffsetToRowStart(5, 0)).toBe(5);
    expect(alignOffsetToRowStart(5, -1)).toBe(5);
  });
});

describe('byteSpanForRows', () => {
  it('returns product for positive inputs', () => {
    expect(byteSpanForRows(10, 16)).toBe(160);
  });

  it('returns zero for non-positive inputs', () => {
    expect(byteSpanForRows(0, 16)).toBe(0);
    expect(byteSpanForRows(4, 0)).toBe(0);
  });
});

describe('computeVisibleHexRows', () => {
  it('emits multiple rows with aligned offsets and bytes per row', () => {
    const data = new Uint8Array([
      0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    ]);
    const rows = computeVisibleHexRows({
      data,
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow: 4,
      viewportRowCount: 2,
      totalByteLength: 8,
    });
    expect(rows).toHaveLength(2);
    expect(rows[0]!.offset).toBe(0);
    expect(rows[0]!.hexPairs).toEqual(['41', '42', '43', '44']);
    expect(rows[0]!.ascii).toBe('ABCD');
    expect(rows[1]!.offset).toBe(4);
    expect(rows[1]!.hexPairs).toEqual(['45', '46', '47', '48']);
    expect(rows[1]!.ascii).toBe('EFGH');
  });

  it('maps bytes to hex pairs and ASCII', () => {
    const data = new Uint8Array([0x48, 0x65, 0x6c, 0x6c, 0x6f]);
    const rows = computeVisibleHexRows({
      data,
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow: 16,
      viewportRowCount: 1,
      totalByteLength: 5,
    });
    expect(rows).toHaveLength(1);
    expect(rows[0]!.offset).toBe(0);
    expect(rows[0]!.hexPairs.slice(0, 5)).toEqual(['48', '65', '6c', '6c', '6f']);
    expect(rows[0]!.ascii.startsWith('Hello')).toBe(true);
  });

  it('pads past totalByteLength with gaps', () => {
    const data = new Uint8Array([0xab]);
    const rows = computeVisibleHexRows({
      data,
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow: 4,
      viewportRowCount: 1,
      totalByteLength: 1,
    });
    expect(rows[0]!.hexPairs).toEqual(['ab', '  ', '  ', '  ']);
    expect(rows[0]!.ascii).toBe('.\x20\x20\x20');
  });

  it('shows gaps outside loaded window', () => {
    const data = new Uint8Array([0x01, 0x02]);
    const rows = computeVisibleHexRows({
      data,
      bufferStartOffset: 100,
      startOffset: 99,
      bytesPerRow: 4,
      viewportRowCount: 1,
      totalByteLength: 200,
    });
    expect(rows[0]!.offset).toBe(99);
    expect(rows[0]!.hexPairs).toEqual(['  ', '01', '02', '  ']);
    expect(rows[0]!.ascii).toBe(' .. ');
  });

  it('returns no rows when bytesPerRow or viewportRowCount is non-positive', () => {
    expect(
      computeVisibleHexRows({
        data: new Uint8Array([1]),
        bufferStartOffset: 0,
        startOffset: 0,
        bytesPerRow: 0,
        viewportRowCount: 3,
      }),
    ).toEqual([]);
    expect(
      computeVisibleHexRows({
        data: new Uint8Array([1]),
        bufferStartOffset: 0,
        startOffset: 0,
        bytesPerRow: 4,
        viewportRowCount: 0,
      }),
    ).toEqual([]);
  });

  it('treats empty data as all gaps when total length is zero', () => {
    const rows = computeVisibleHexRows({
      data: new Uint8Array(),
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow: 4,
      viewportRowCount: 1,
    });
    expect(rows[0]!.hexPairs).toEqual(['  ', '  ', '  ', '  ']);
    expect(rows[0]!.ascii).toBe('    ');
  });

  it('clips rendering when totalByteLength is shorter than loaded data', () => {
    const data = new Uint8Array([0xaa, 0xbb, 0xcc, 0xdd]);
    const rows = computeVisibleHexRows({
      data,
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow: 4,
      viewportRowCount: 1,
      totalByteLength: 2,
    });
    expect(rows[0]!.hexPairs).toEqual(['aa', 'bb', '  ', '  ']);
    expect(rows[0]!.ascii).toBe('..\x20\x20');
  });

  it('clamps negative totalByteLength to zero', () => {
    const rows = computeVisibleHexRows({
      data: new Uint8Array([0x01]),
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow: 2,
      viewportRowCount: 1,
      totalByteLength: -10,
    });
    expect(rows[0]!.hexPairs).toEqual(['  ', '  ']);
    expect(rows[0]!.ascii).toBe('  ');
  });

  it('places last in-range byte on the last column of the row boundary', () => {
    const data = new Uint8Array([0x10, 0x20, 0x30]);
    const rows = computeVisibleHexRows({
      data,
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow: 3,
      viewportRowCount: 1,
      totalByteLength: 3,
    });
    expect(rows[0]!.hexPairs).toEqual(['10', '20', '30']);
    expect(rows[0]!.offset).toBe(0);
  });

  it('keeps row width stable for large bytesPerRow', () => {
    const bytesPerRow = 256;
    const data = new Uint8Array(1);
    data[0] = 0x42;
    const rows = computeVisibleHexRows({
      data,
      bufferStartOffset: 0,
      startOffset: 0,
      bytesPerRow,
      viewportRowCount: 1,
      totalByteLength: 1,
    });
    expect(rows[0]!.hexPairs).toHaveLength(bytesPerRow);
    expect(rows[0]!.hexPairs[0]).toBe('42');
    expect(rows[0]!.hexPairs[bytesPerRow - 1]).toBe('  ');
    expect(rows[0]!.ascii).toHaveLength(bytesPerRow);
    expect(rows[0]!.ascii[0]).toBe('B');
    expect(rows[0]!.ascii[bytesPerRow - 1]).toBe(' ');
  });
});
