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
