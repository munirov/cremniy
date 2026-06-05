/**
 * ASCII string extraction (Qt parity: Strings tool tab).
 *
 * Scans a byte buffer for runs of ≥ minLength printable ASCII characters and
 * emits them with their starting file offset. Mirrors the behaviour of
 * `strings(1)` for the simple ASCII case (UTF-16 strings are out of scope for
 * the first cut).
 */

export type ExtractedString = Readonly<{
  offset: number;
  length: number;
  text: string;
}>;

const DEFAULT_MIN_LENGTH = 4;
const MAX_STRINGS = 5000;

function isPrintable(byte: number): boolean {
  return byte >= 0x20 && byte <= 0x7e;
}

export function extractAsciiStrings(
  bytes: Uint8Array,
  minLength: number = DEFAULT_MIN_LENGTH,
): ExtractedString[] {
  const out: ExtractedString[] = [];
  let runStart = -1;
  for (let i = 0; i <= bytes.length; i += 1) {
    const b = i < bytes.length ? bytes[i]! : -1;
    if (b !== -1 && isPrintable(b)) {
      if (runStart < 0) {
        runStart = i;
      }
      continue;
    }
    if (runStart >= 0) {
      const runLength = i - runStart;
      if (runLength >= minLength) {
        out.push({
          offset: runStart,
          length: runLength,
          text: new TextDecoder('ascii').decode(bytes.subarray(runStart, i)),
        });
        if (out.length >= MAX_STRINGS) {
          return out;
        }
      }
      runStart = -1;
    }
  }
  return out;
}

export function filterStrings(items: ExtractedString[], query: string): ExtractedString[] {
  const q = query.trim().toLowerCase();
  if (q === '') {
    return items;
  }
  return items.filter((s) => s.text.toLowerCase().includes(q));
}
