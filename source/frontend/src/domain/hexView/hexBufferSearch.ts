export type ParseOffsetResult =
  | { ok: true; value: number }
  | { ok: false; message: string };

export type ParseHexBytesResult =
  | { ok: true; bytes: Uint8Array }
  | { ok: false; message: string };

/**
 * Returns start indices of every occurrence of `needle` in `haystack`.
 * Empty `needle` yields no matches (no vacuous matches).
 */
export function findAllSubsequenceIndices(haystack: Uint8Array, needle: Uint8Array): number[] {
  if (needle.length === 0 || needle.length > haystack.length) {
    return [];
  }
  const out: number[] = [];
  const lastStart = haystack.length - needle.length;
  for (let i = 0; i <= lastStart; i += 1) {
    let match = true;
    for (let j = 0; j < needle.length; j += 1) {
      if (haystack[i + j] !== needle[j]) {
        match = false;
        break;
      }
    }
    if (match) {
      out.push(i);
    }
  }
  return out;
}

function hexStringToSafeInteger(digits: string): number | null {
  if (digits.length === 0) {
    return null;
  }
  try {
    const bi = BigInt(`0x${digits}`);
    if (bi < 0n || bi > BigInt(Number.MAX_SAFE_INTEGER)) {
      return null;
    }
    return Number(bi);
  } catch {
    return null;
  }
}

/**
 * Parses a user offset for “go to”.
 * Hex mode: optional `0x` prefix; whitespace between digit groups is ignored.
 * Decimal mode: digits only (non-negative).
 */
export function parseOffsetInput(input: string, radix: 'hex' | 'decimal'): ParseOffsetResult {
  const trimmed = input.trim();
  if (trimmed === '') {
    return { ok: false, message: 'Enter an offset.' };
  }

  if (radix === 'decimal') {
    if (!/^\d+$/.test(trimmed)) {
      return { ok: false, message: 'Decimal offset must be non-negative digits only.' };
    }
    const value = Number(trimmed);
    if (!Number.isSafeInteger(value)) {
      return { ok: false, message: 'Offset is too large.' };
    }
    return { ok: true, value };
  }

  let body = trimmed.replace(/^0x/i, '');
  body = body.replace(/\s+/g, '');
  if (body === '') {
    return { ok: false, message: 'Enter a hex offset.' };
  }
  if (!/^[0-9a-fA-F]+$/.test(body)) {
    return { ok: false, message: 'Hex offset must contain hex digits only.' };
  }

  const value = hexStringToSafeInteger(body);
  if (value === null) {
    return { ok: false, message: 'Offset is too large.' };
  }
  return { ok: true, value };
}

/**
 * Parses space-separated hex pairs and/or a continuous hex digit string into bytes.
 * Any non-hex characters are stripped (e.g. spaces, `0x`, commas).
 */
export function parseHexByteSequence(input: string): ParseHexBytesResult {
  const withoutPrefixes = input.replace(/0x/gi, '');
  const digits = withoutPrefixes.replace(/[^0-9a-fA-F]/g, '');
  if (digits.length === 0) {
    return { ok: false, message: 'Enter hex bytes (e.g. 41 42 or 4142).' };
  }
  if (digits.length % 2 !== 0) {
    return { ok: false, message: 'Hex bytes need an even number of digits.' };
  }
  const out = new Uint8Array(digits.length / 2);
  for (let i = 0; i < out.length; i += 1) {
    out[i] = Number.parseInt(digits.slice(i * 2, i * 2 + 2), 16);
  }
  return { ok: true, bytes: out };
}

// --- 4-режима поиска (Qt parity: HexFindDialog) ------------------------------

export type IntWidth = 8 | 16 | 32 | 64;
export type FloatWidth = 32 | 64;
export type Endian = 'little' | 'big';
export type SearchDirection = 'forward' | 'backward';

/**
 * UTF-8 text → bytes. Case-insensitive переключается на стороне поиска через
 * нормализацию обеих сторон (но мы возвращаем raw UTF-8 без casing).
 */
export function parseTextSearch(text: string): ParseHexBytesResult {
  if (text.length === 0) {
    return { ok: false, message: 'Enter text to find.' };
  }
  const bytes = new TextEncoder().encode(text);
  return { ok: true, bytes };
}

/**
 * Сравнение байт с учётом case-insensitive (только для ASCII).
 */
function asciiLowerByte(b: number): number {
  return b >= 65 && b <= 90 ? b + 32 : b;
}

/**
 * Знаковая/беззнаковая дешифровка целого, упаковка в N байт по endian.
 * Все вычисления через BigInt, чтобы 64-битные значения не теряли точность.
 */
export function parseIntSearch(
  input: string,
  width: IntWidth,
  endian: Endian,
  signed: boolean,
): ParseHexBytesResult {
  const trimmed = input.trim();
  if (trimmed === '') {
    return { ok: false, message: 'Enter an integer to find.' };
  }
  let bi: bigint;
  try {
    if (/^-?0x[0-9a-fA-F]+$/i.test(trimmed)) {
      const neg = trimmed.startsWith('-');
      bi = neg ? -BigInt(trimmed.slice(1)) : BigInt(trimmed);
    } else if (/^-?\d+$/.test(trimmed)) {
      bi = BigInt(trimmed);
    } else {
      return { ok: false, message: 'Integer must be decimal or 0x-hex.' };
    }
  } catch {
    return { ok: false, message: 'Could not parse the integer.' };
  }

  const byteCount = width / 8;
  const max = signed ? (1n << BigInt(width - 1)) - 1n : (1n << BigInt(width)) - 1n;
  const min = signed ? -(1n << BigInt(width - 1)) : 0n;
  if (bi < min || bi > max) {
    return {
      ok: false,
      message: `Value out of range for ${signed ? 'signed' : 'unsigned'} ${width}-bit.`,
    };
  }
  if (bi < 0n) {
    bi += 1n << BigInt(width);
  }

  const bytes = new Uint8Array(byteCount);
  for (let i = 0; i < byteCount; i += 1) {
    const slot = endian === 'little' ? i : byteCount - 1 - i;
    bytes[slot] = Number(bi & 0xffn);
    bi >>= 8n;
  }
  return { ok: true, bytes };
}

/**
 * Float / Double литерал → IEEE-754 байты.
 */
export function parseFloatSearch(
  input: string,
  width: FloatWidth,
  endian: Endian,
): ParseHexBytesResult {
  const trimmed = input.trim();
  if (trimmed === '') {
    return { ok: false, message: 'Enter a float value to find.' };
  }
  const num = Number(trimmed);
  if (!Number.isFinite(num)) {
    return { ok: false, message: 'Could not parse float.' };
  }
  const buf = new ArrayBuffer(width / 8);
  const view = new DataView(buf);
  const littleEndian = endian === 'little';
  if (width === 32) {
    view.setFloat32(0, num, littleEndian);
  } else {
    view.setFloat64(0, num, littleEndian);
  }
  return { ok: true, bytes: new Uint8Array(buf) };
}

/**
 * Поиск следующего/предыдущего вхождения от `cursorOffset`.
 * Возвращает -1 если не найдено. caseInsensitive работает только в режиме Text.
 */
export function findOccurrence(
  haystack: Uint8Array,
  needle: Uint8Array,
  cursorOffset: number,
  direction: SearchDirection,
  caseInsensitive = false,
): number {
  if (needle.length === 0 || needle.length > haystack.length) {
    return -1;
  }
  const lastStart = haystack.length - needle.length;

  if (direction === 'forward') {
    const startFrom = Math.max(0, Math.min(cursorOffset, lastStart));
    for (let i = startFrom; i <= lastStart; i += 1) {
      if (matchesAt(haystack, needle, i, caseInsensitive)) {
        return i;
      }
    }
    return -1;
  }

  const startFrom = Math.min(lastStart, Math.max(0, cursorOffset - 1));
  for (let i = startFrom; i >= 0; i -= 1) {
    if (matchesAt(haystack, needle, i, caseInsensitive)) {
      return i;
    }
  }
  return -1;
}

function matchesAt(
  haystack: Uint8Array,
  needle: Uint8Array,
  offset: number,
  caseInsensitive: boolean,
): boolean {
  for (let j = 0; j < needle.length; j += 1) {
    const h = haystack[offset + j];
    const n = needle[j];
    if (caseInsensitive ? asciiLowerByte(h) !== asciiLowerByte(n) : h !== n) {
      return false;
    }
  }
  return true;
}

/**
 * Заменяет диапазон [offset, offset+oldBytes.length) на newBytes.
 * Возвращает новый буфер. Длины могут отличаться (insert/remove/replace).
 */
export function replaceRange(
  buffer: Uint8Array,
  offset: number,
  oldLength: number,
  newBytes: Uint8Array,
): Uint8Array {
  const tail = buffer.subarray(offset + oldLength);
  const out = new Uint8Array(offset + newBytes.length + tail.length);
  out.set(buffer.subarray(0, offset), 0);
  out.set(newBytes, offset);
  out.set(tail, offset + newBytes.length);
  return out;
}
