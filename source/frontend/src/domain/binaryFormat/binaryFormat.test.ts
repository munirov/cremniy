import { describe, expect, it } from 'vitest';

import { analyzeBinaryFormat } from './binaryFormat';

describe('binaryFormat', () => {
  it('uses RAW as the safe fallback for unknown bytes', () => {
    const analysis = analyzeBinaryFormat(new Uint8Array([0xde, 0xad, 0xbe, 0xef]));

    expect(analysis.detected).toBe('unknown');
    expect(analysis.pages.raw.status).toBe('supported');
    expect(analysis.pages.raw.fields).toEqual([
      { label: 'Size', value: '4 bytes' },
      { label: 'Byte count', value: '4' },
    ]);
  });

  it('treats truncated format headers as unsupported without throwing', () => {
    const truncatedHeaders = [
      {
        bytes: new Uint8Array([0x7f, 0x45, 0x4c, 0x46]),
        page: 'elf' as const,
        message: 'ELF signature found, but ident fields are truncated. Expected at least 6 bytes.',
      },
      {
        bytes: new Uint8Array([0x7f, 0x45, 0x4c, 0x46, 0x02]),
        page: 'elf' as const,
        message: 'ELF signature found, but ident fields are truncated. Expected at least 6 bytes.',
      },
      {
        bytes: new Uint8Array([0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01]),
        page: 'elf' as const,
        message: 'ELF signature found, but header is shorter than 64 bytes.',
      },
      {
        bytes: new Uint8Array([0x4d, 0x5a]),
        page: 'pe' as const,
        message: 'MZ signature found, but DOS header is shorter than 64 bytes.',
      },
      {
        bytes: new Uint8Array(511),
        page: 'mbr' as const,
        message: 'Buffer is shorter than 512 bytes.',
      },
    ];

    for (const { bytes, page, message } of truncatedHeaders) {
      expect(() => analyzeBinaryFormat(bytes)).not.toThrow();

      const analysis = analyzeBinaryFormat(bytes);

      expect(analysis.detected).toBe('unknown');
      expect(analysis.pages[page].status).toBe('unsupported');
      expect(analysis.pages[page].message).toBe(message);
      expect(analysis.pages.raw.status).toBe('supported');
    }
  });

  it('parses a little-endian ELF64 header summary', () => {
    const bytes = new Uint8Array(64);
    bytes.set([0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01]);
    bytes.set([0x03, 0x00], 16);
    bytes.set([0x3e, 0x00], 18);
    bytes.set([0x80, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00], 24);

    const analysis = analyzeBinaryFormat(bytes);

    expect(analysis.detected).toBe('elf');
    expect(analysis.pages.elf.status).toBe('supported');
    expect(analysis.pages.elf.fields).toEqual([
      { label: 'Magic', value: '7F 45 4C 46' },
      { label: 'Class', value: '64-bit' },
      { label: 'Endian', value: 'Little endian' },
      { label: 'Type', value: 'Shared object (ET_DYN)' },
      { label: 'Machine', value: 'x86-64' },
      { label: 'Entry', value: '0x0000000000400080' },
    ]);
  });

  it('rejects ELF headers with unsupported class values', () => {
    const bytes = new Uint8Array(64);
    bytes.set([0x7f, 0x45, 0x4c, 0x46, 0x03, 0x01]);

    const analysis = analyzeBinaryFormat(bytes);

    expect(analysis.detected).toBe('unknown');
    expect(analysis.pages.elf.status).toBe('unsupported');
    expect(analysis.pages.elf.message).toBe(
      'ELF signature found, but class 0x03 is unsupported. Expected 0x01 (32-bit) or 0x02 (64-bit).',
    );
  });

  it('rejects ELF headers with unsupported data encoding values', () => {
    const bytes = new Uint8Array(64);
    bytes.set([0x7f, 0x45, 0x4c, 0x46, 0x02, 0x03]);

    const analysis = analyzeBinaryFormat(bytes);

    expect(analysis.detected).toBe('unknown');
    expect(analysis.pages.elf.status).toBe('unsupported');
    expect(analysis.pages.elf.message).toBe(
      'ELF signature found, but data encoding 0x03 is unsupported. Expected 0x01 (little endian) or 0x02 (big endian).',
    );
  });

  it('parses a PE header summary', () => {
    const bytes = new Uint8Array(160);
    bytes.set([0x4d, 0x5a]);
    bytes.set([0x80, 0x00, 0x00, 0x00], 0x3c);
    bytes.set([0x50, 0x45, 0x00, 0x00], 0x80);
    bytes.set([0x64, 0x86], 0x84);
    bytes.set([0x03, 0x00], 0x86);
    bytes.set([0xf0, 0x00], 0x94);
    bytes.set([0x0b, 0x02], 0x98);

    const analysis = analyzeBinaryFormat(bytes);

    expect(analysis.detected).toBe('pe');
    expect(analysis.pages.pe.status).toBe('supported');
    expect(analysis.pages.pe.fields).toContainEqual({
      label: 'Machine',
      value: 'x86-64 (AMD64)',
    });
    expect(analysis.pages.pe.fields).toContainEqual({ label: 'Sections', value: '3' });
    expect(analysis.pages.pe.fields).toContainEqual({
      label: 'Optional header magic',
      value: 'PE32+ (0x020B)',
    });
  });

  it('rejects a PE header when e_lfanew points outside the buffer', () => {
    const bytes = new Uint8Array(128);
    bytes.set([0x4d, 0x5a]);
    bytes.set([0xff, 0xff, 0xff, 0xff], 0x3c);

    const analysis = analyzeBinaryFormat(bytes);

    expect(analysis.detected).toBe('unknown');
    expect(analysis.pages.pe.status).toBe('unsupported');
    expect(analysis.pages.pe.message).toBe(
      'MZ signature found, but PE header offset is outside the buffer.',
    );
  });

  it('parses an MBR partition summary', () => {
    const bytes = new Uint8Array(512);
    bytes[510] = 0x55;
    bytes[511] = 0xaa;
    bytes.set([0x80, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00], 0x1be);
    bytes.set([0x00, 0x08, 0x00, 0x00], 0x1be + 8);
    bytes.set([0x00, 0x10, 0x00, 0x00], 0x1be + 12);
    bytes.set([0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00], 0x1be + 16);
    bytes.set([0x3f, 0x00, 0x00, 0x00], 0x1be + 16 + 8);
    bytes.set([0x00, 0x08, 0x00, 0x00], 0x1be + 16 + 12);
    bytes.set([0x00, 0x00, 0x00, 0x00, 0xee, 0x00, 0x00, 0x00], 0x1be + 48);
    bytes.set([0x01, 0x00, 0x00, 0x00], 0x1be + 48 + 8);
    bytes.set([0xff, 0xff, 0xff, 0xff], 0x1be + 48 + 12);

    const analysis = analyzeBinaryFormat(bytes);

    expect(analysis.detected).toBe('mbr');
    expect(analysis.pages.mbr.status).toBe('supported');
    expect(analysis.pages.mbr.partitions).toEqual([
      {
        index: 1,
        active: 'Yes',
        type: '0x83',
        startLba: 2048,
        sectorCount: 4096,
        description: 'Linux',
      },
      {
        index: 2,
        active: 'No',
        type: '0x07',
        startLba: 63,
        sectorCount: 2048,
        description: 'NTFS / exFAT',
      },
      {
        index: 3,
        active: 'No',
        type: '0x00',
        startLba: 0,
        sectorCount: 0,
        description: 'Empty',
      },
      {
        index: 4,
        active: 'No',
        type: '0xEE',
        startLba: 1,
        sectorCount: 4294967295,
        description: 'GPT protective',
      },
    ]);
  });
});
