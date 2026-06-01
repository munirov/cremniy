// Auto-comment string references in the disassembly (Qt parity: ; "Hello").
// Qt got strings from radare2 (izj). With objdump we extract ASCII strings from
// the file ourselves and map an instruction's referenced virtual address to a
// string via the section vaddr/fileOffset table objdump already gives us.

import type { DisassemblySection } from './disassembly';

export type ExtractedString = {
  /** File offset of the first byte of the run. */
  fileOffset: number;
  value: string;
};

const DEFAULT_MIN_LENGTH = 4;

/** Extract NUL-terminated printable-ASCII runs of at least `minLength` chars. */
export function extractAsciiStrings(
  bytes: Uint8Array,
  minLength = DEFAULT_MIN_LENGTH,
): ExtractedString[] {
  const out: ExtractedString[] = [];
  let start = -1;
  let chars: string[] = [];

  const flush = (endIndex: number, nulTerminated: boolean) => {
    if (start >= 0 && nulTerminated && chars.length >= minLength) {
      out.push({ fileOffset: start, value: chars.join('') });
    }
    start = -1;
    chars = [];
    void endIndex;
  };

  for (let i = 0; i < bytes.length; i += 1) {
    const b = bytes[i]!;
    const printable = b >= 0x20 && b <= 0x7e;
    if (printable) {
      if (start < 0) {
        start = i;
      }
      chars.push(String.fromCharCode(b));
    } else {
      // Only count runs that end with a NUL terminator (C-string convention).
      flush(i, b === 0x00);
    }
  }
  return out;
}

/** Convert a virtual address to a file offset using the containing section. */
export function vaddrToFileOffset(
  vaddr: number,
  sections: readonly DisassemblySection[],
): number | null {
  for (const section of sections) {
    if (
      section.vaddr == null ||
      section.fileOffset == null ||
      section.size == null ||
      !section.hasFileMapping
    ) {
      continue;
    }
    if (vaddr >= section.vaddr && vaddr < section.vaddr + section.size) {
      return section.fileOffset + (vaddr - section.vaddr);
    }
  }
  return null;
}

/** Index extracted strings by file offset for O(1) lookup. */
export function indexStringsByFileOffset(
  strings: readonly ExtractedString[],
): Map<number, string> {
  const map = new Map<number, string>();
  for (const s of strings) {
    if (!map.has(s.fileOffset)) {
      map.set(s.fileOffset, s.value);
    }
  }
  return map;
}

const ABSOLUTE_HEX_RE = /\b0x([0-9a-fA-F]+)\b/g;

function renderComment(value: string): string {
  let v = value.replace(/\n/g, '\\n').replace(/\r/g, '\\r');
  if (v.length > 80) {
    v = `${v.slice(0, 80)}…`;
  }
  return `"${v}"`;
}

/**
 * Resolve a `; "string"` auto-comment for an instruction's operands by matching
 * any absolute hex address that maps (via sections) to an extracted string.
 * Returns null when nothing resolves (mirrors Qt's absolute-address case).
 */
export function resolveStringComment(
  operands: string,
  sections: readonly DisassemblySection[],
  stringsByFileOffset: Map<number, string>,
): string | null {
  for (const match of operands.matchAll(ABSOLUTE_HEX_RE)) {
    const vaddr = Number.parseInt(match[1]!, 16);
    if (!Number.isFinite(vaddr)) {
      continue;
    }
    const fileOffset = vaddrToFileOffset(vaddr, sections);
    if (fileOffset == null) {
      continue;
    }
    const value = stringsByFileOffset.get(fileOffset);
    if (value != null && value !== '') {
      return renderComment(value);
    }
  }
  return null;
}
