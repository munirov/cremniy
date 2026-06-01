export type BinaryFormatId = 'raw' | 'elf' | 'pe' | 'mbr';

export type BinaryFormatDetection = BinaryFormatId | 'unknown';

export type BinaryFormatField = Readonly<{
  label: string;
  value: string;
}>;

export type BinaryFormatPartition = Readonly<{
  index: number;
  active: string;
  type: string;
  startLba: number;
  sectorCount: number;
  description: string;
}>;

export type BinaryFormatPageSummary = Readonly<{
  id: BinaryFormatId;
  label: string;
  status: 'supported' | 'unsupported';
  message: string;
  fields: readonly BinaryFormatField[];
  partitions?: readonly BinaryFormatPartition[];
}>;

export type BinaryFormatAnalysis = Readonly<{
  detected: BinaryFormatDetection;
  pages: Record<BinaryFormatId, BinaryFormatPageSummary>;
}>;

const ELF_MAGIC = [0x7f, 0x45, 0x4c, 0x46] as const;
const PE_SIGNATURE = [0x50, 0x45, 0x00, 0x00] as const;
const MBR_MIN_BYTES = 512;
const MBR_PARTITION_OFFSET = 0x1be;
const MBR_PARTITION_SIZE = 16;

export function analyzeBinaryFormat(bytes: Uint8Array): BinaryFormatAnalysis {
  const pages = {
    raw: parseRaw(bytes),
    elf: parseElf(bytes),
    pe: parsePe(bytes),
    mbr: parseMbr(bytes),
  };

  return {
    detected: detectFormat(pages),
    pages,
  };
}

function detectFormat(pages: Record<BinaryFormatId, BinaryFormatPageSummary>): BinaryFormatDetection {
  if (pages.elf.status === 'supported') {
    return 'elf';
  }
  if (pages.pe.status === 'supported') {
    return 'pe';
  }
  if (pages.mbr.status === 'supported') {
    return 'mbr';
  }
  return 'unknown';
}

function parseRaw(bytes: Uint8Array): BinaryFormatPageSummary {
  return {
    id: 'raw',
    label: 'RAW',
    status: 'supported',
    message: 'Raw byte stream fallback.',
    fields: [
      { label: 'Size', value: formatBytes(bytes.length) },
      { label: 'Byte count', value: bytes.length.toString() },
    ],
  };
}

function parseElf(bytes: Uint8Array): BinaryFormatPageSummary {
  if (!hasPrefix(bytes, ELF_MAGIC)) {
    return unsupported('elf', 'ELF', 'No ELF signature (7F 45 4C 46).');
  }
  if (bytes.length < 6) {
    return unsupported(
      'elf',
      'ELF',
      'ELF signature found, but ident fields are truncated. Expected at least 6 bytes.',
    );
  }

  const elfClass = bytes[4];
  const endian = bytes[5];
  if (elfClass !== 1 && elfClass !== 2) {
    return unsupported(
      'elf',
      'ELF',
      `ELF signature found, but class ${formatHexNumber(elfClass, 2)} is unsupported. Expected 0x01 (32-bit) or 0x02 (64-bit).`,
    );
  }
  if (endian !== 1 && endian !== 2) {
    return unsupported(
      'elf',
      'ELF',
      `ELF signature found, but data encoding ${formatHexNumber(endian, 2)} is unsupported. Expected 0x01 (little endian) or 0x02 (big endian).`,
    );
  }

  const is64Bit = elfClass === 2;
  const minHeaderLength = is64Bit ? 64 : 52;
  if (bytes.length < minHeaderLength) {
    return unsupported('elf', 'ELF', `ELF signature found, but header is shorter than ${minHeaderLength} bytes.`);
  }

  const readU16 = endian === 2 ? readU16Be : readU16Le;
  const entry = is64Bit ? readU64(bytes, 24, endian === 2) : BigInt(readU32(bytes, 24, endian === 2));
  const type = readU16(bytes, 16);
  const machine = readU16(bytes, 18);

  return {
    id: 'elf',
    label: 'ELF',
    status: 'supported',
    message: 'ELF header detected.',
    fields: [
      { label: 'Magic', value: '7F 45 4C 46' },
      { label: 'Class', value: formatElfClass(elfClass) },
      { label: 'Endian', value: formatElfEndian(endian) },
      { label: 'Type', value: formatElfType(type) },
      { label: 'Machine', value: formatElfMachine(machine) },
      { label: 'Entry', value: formatHexBigInt(entry, is64Bit ? 16 : 8) },
    ],
  };
}

function parsePe(bytes: Uint8Array): BinaryFormatPageSummary {
  if (bytes.length < 2 || bytes[0] !== 0x4d || bytes[1] !== 0x5a) {
    return unsupported('pe', 'PE', 'No DOS MZ signature.');
  }
  if (bytes.length < 0x40) {
    return unsupported('pe', 'PE', 'MZ signature found, but DOS header is shorter than 64 bytes.');
  }

  const peOffset = readU32Le(bytes, 0x3c);
  if (!Number.isSafeInteger(peOffset) || peOffset + 24 > bytes.length) {
    return unsupported('pe', 'PE', 'MZ signature found, but PE header offset is outside the buffer.');
  }
  if (!hasBytesAt(bytes, peOffset, PE_SIGNATURE)) {
    return unsupported('pe', 'PE', 'MZ signature found, but PE signature is missing.');
  }

  const machine = readU16Le(bytes, peOffset + 4);
  const sections = readU16Le(bytes, peOffset + 6);
  const optionalHeaderSize = readU16Le(bytes, peOffset + 20);
  const optionalHeaderOffset = peOffset + 24;
  const optionalHeaderMagic =
    optionalHeaderSize >= 2 && optionalHeaderOffset + 2 <= bytes.length
      ? formatPeOptionalHeaderMagic(readU16Le(bytes, optionalHeaderOffset))
      : 'Not present';

  return {
    id: 'pe',
    label: 'PE',
    status: 'supported',
    message: 'Portable Executable header detected.',
    fields: [
      { label: 'DOS signature', value: 'MZ' },
      { label: 'PE signature offset', value: formatHexNumber(peOffset, 8) },
      { label: 'Machine', value: formatPeMachine(machine) },
      { label: 'Sections', value: sections.toString() },
      { label: 'Optional header magic', value: optionalHeaderMagic },
    ],
  };
}

function parseMbr(bytes: Uint8Array): BinaryFormatPageSummary {
  if (bytes.length < MBR_MIN_BYTES) {
    return unsupported('mbr', 'MBR', 'Buffer is shorter than 512 bytes.');
  }
  if (bytes[510] !== 0x55 || bytes[511] !== 0xaa) {
    return unsupported('mbr', 'MBR', 'No boot signature 55 AA at offsets 510-511.');
  }

  const partitions = Array.from({ length: 4 }, (_, index) => {
    const offset = MBR_PARTITION_OFFSET + index * MBR_PARTITION_SIZE;
    const bootFlag = bytes[offset]!;
    const type = bytes[offset + 4]!;
    return {
      index: index + 1,
      active: formatMbrBootFlag(bootFlag),
      type: formatHexNumber(type, 2),
      startLba: readU32Le(bytes, offset + 8),
      sectorCount: readU32Le(bytes, offset + 12),
      description: formatMbrPartitionType(type),
    };
  });

  return {
    id: 'mbr',
    label: 'MBR',
    status: 'supported',
    message: 'Master Boot Record signature detected.',
    fields: [
      { label: 'Boot signature', value: '55 AA' },
      { label: 'Partition entries', value: partitions.length.toString() },
    ],
    partitions,
  };
}

function unsupported(
  id: BinaryFormatId,
  label: string,
  message: string,
): BinaryFormatPageSummary {
  return { id, label, status: 'unsupported', message, fields: [] };
}

function hasPrefix(bytes: Uint8Array, prefix: readonly number[]): boolean {
  return hasBytesAt(bytes, 0, prefix);
}

function hasBytesAt(bytes: Uint8Array, offset: number, expected: readonly number[]): boolean {
  if (offset < 0 || offset + expected.length > bytes.length) {
    return false;
  }
  return expected.every((value, index) => bytes[offset + index] === value);
}

function readU16Le(bytes: Uint8Array, offset: number): number {
  return bytes[offset]! | (bytes[offset + 1]! << 8);
}

function readU16Be(bytes: Uint8Array, offset: number): number {
  return (bytes[offset]! << 8) | bytes[offset + 1]!;
}

function readU32Le(bytes: Uint8Array, offset: number): number {
  return (
    bytes[offset]! |
    (bytes[offset + 1]! << 8) |
    (bytes[offset + 2]! << 16) |
    (bytes[offset + 3]! << 24)
  ) >>> 0;
}

function readU32Be(bytes: Uint8Array, offset: number): number {
  return (
    ((bytes[offset]! << 24) |
      (bytes[offset + 1]! << 16) |
      (bytes[offset + 2]! << 8) |
      bytes[offset + 3]!) >>>
    0
  );
}

function readU32(bytes: Uint8Array, offset: number, bigEndian: boolean): number {
  return bigEndian ? readU32Be(bytes, offset) : readU32Le(bytes, offset);
}

function readU64(bytes: Uint8Array, offset: number, bigEndian: boolean): bigint {
  const high = BigInt(readU32(bytes, bigEndian ? offset : offset + 4, bigEndian));
  const low = BigInt(readU32(bytes, bigEndian ? offset + 4 : offset, bigEndian));
  return (high << 32n) | low;
}

function formatBytes(length: number): string {
  return length === 1 ? '1 byte' : `${length} bytes`;
}

function formatHexNumber(value: number, width: number): string {
  return `0x${value.toString(16).toUpperCase().padStart(width, '0')}`;
}

function formatHexBigInt(value: bigint, width: number): string {
  return `0x${value.toString(16).toUpperCase().padStart(width, '0')}`;
}

function formatElfClass(value: number | undefined): string {
  if (value === 1) {
    return '32-bit';
  }
  if (value === 2) {
    return '64-bit';
  }
  return value == null ? 'Unknown' : `Unknown (${formatHexNumber(value, 2)})`;
}

function formatElfEndian(value: number | undefined): string {
  if (value === 1) {
    return 'Little endian';
  }
  if (value === 2) {
    return 'Big endian';
  }
  return value == null ? 'Unknown' : `Unknown (${formatHexNumber(value, 2)})`;
}

function formatElfType(value: number): string {
  switch (value) {
    case 1:
      return 'Relocatable (ET_REL)';
    case 2:
      return 'Executable (ET_EXEC)';
    case 3:
      return 'Shared object (ET_DYN)';
    case 4:
      return 'Core (ET_CORE)';
    default:
      return formatHexNumber(value, 4);
  }
}

function formatElfMachine(value: number): string {
  switch (value) {
    case 0x03:
      return 'x86';
    case 0x28:
      return 'ARM';
    case 0x3e:
      return 'x86-64';
    case 0xb7:
      return 'AArch64';
    default:
      return formatHexNumber(value, 4);
  }
}

function formatPeMachine(value: number): string {
  switch (value) {
    case 0x014c:
      return 'x86 (i386)';
    case 0x8664:
      return 'x86-64 (AMD64)';
    case 0xaa64:
      return 'AArch64';
    default:
      return formatHexNumber(value, 4);
  }
}

function formatPeOptionalHeaderMagic(value: number): string {
  switch (value) {
    case 0x010b:
      return 'PE32 (0x010B)';
    case 0x020b:
      return 'PE32+ (0x020B)';
    case 0x0107:
      return 'ROM image (0x0107)';
    default:
      return formatHexNumber(value, 4);
  }
}

function formatMbrBootFlag(value: number): string {
  if (value === 0x80) {
    return 'Yes';
  }
  if (value === 0x00) {
    return 'No';
  }
  return formatHexNumber(value, 2);
}

function formatMbrPartitionType(value: number): string {
  switch (value) {
    case 0x00:
      return 'Empty';
    case 0x01:
      return 'FAT12';
    case 0x04:
    case 0x06:
    case 0x0e:
      return 'FAT16';
    case 0x07:
      return 'NTFS / exFAT';
    case 0x0b:
    case 0x0c:
      return 'FAT32';
    case 0x83:
      return 'Linux';
    case 0xee:
      return 'GPT protective';
    default:
      return 'Unknown';
  }
}
