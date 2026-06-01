const HEX = '0123456789abcdef';

export type HexRow = {
  /** Absolute offset of the first column in this row. */
  offset: number;
  /** One two-character lowercase hex pair per column (`bytesPerRow` entries). */
  hexPairs: readonly string[];
  /** One character per column; non-printable bytes render as `.`; gaps use a space. */
  ascii: string;
};

/**
 * Inputs for {@link computeVisibleHexRows}. Callers should use non-negative
 * `bufferStartOffset`, `startOffset`, and `totalByteLength` when provided;
 * the implementation clamps a negative `totalByteLength` to zero.
 */
export type ComputeVisibleHexRowsOptions = {
  data: Uint8Array;
  /** Absolute file offset of `data[0]` (non-negative). */
  bufferStartOffset: number;
  /** Absolute offset of the first byte in the top visible row (typically row-aligned). */
  startOffset: number;
  bytesPerRow: number;
  /** Number of rows to emit (viewport height in rows). */
  viewportRowCount: number;
  /** Total content length; defaults to `bufferStartOffset + data.length`. */
  totalByteLength?: number;
};

export function alignOffsetToRowStart(offset: number, bytesPerRow: number): number {
  if (bytesPerRow <= 0) {
    return offset;
  }
  const aligned = offset - (offset % bytesPerRow);
  return aligned < 0 ? 0 : aligned;
}

export function byteToHexPair(byte: number): string {
  const b = byte & 0xff;
  return `${HEX[b >>> 4]}${HEX[b & 0x0f]}`;
}

export function byteToAsciiColumnChar(byte: number): string {
  const b = byte & 0xff;
  if (b >= 0x20 && b <= 0x7e) {
    return String.fromCharCode(b);
  }
  return '.';
}

/** Bytes spanned by a fixed grid of rows (for paging / fetch sizing). */
export function byteSpanForRows(rowCount: number, bytesPerRow: number): number {
  if (rowCount <= 0 || bytesPerRow <= 0) {
    return 0;
  }
  return rowCount * bytesPerRow;
}

function gapHexPair(): string {
  return '  ';
}

function gapAsciiChar(): string {
  return ' ';
}

export function computeVisibleHexRows(options: ComputeVisibleHexRowsOptions): HexRow[] {
  const {
    data,
    bufferStartOffset,
    startOffset,
    bytesPerRow,
    viewportRowCount,
    totalByteLength: totalByteLengthOpt,
  } = options;

  if (bytesPerRow <= 0 || viewportRowCount <= 0) {
    return [];
  }

  const bufferEndOffset = bufferStartOffset + data.length;
  const totalByteLength =
    totalByteLengthOpt === undefined ? bufferEndOffset : Math.max(0, totalByteLengthOpt);

  const rows: HexRow[] = [];
  for (let r = 0; r < viewportRowCount; r += 1) {
    const rowOffset = startOffset + r * bytesPerRow;
    const hexPairs: string[] = [];
    let ascii = '';

    for (let c = 0; c < bytesPerRow; c += 1) {
      const abs = rowOffset + c;
      if (abs >= totalByteLength) {
        hexPairs.push(gapHexPair());
        ascii += gapAsciiChar();
        continue;
      }
      if (abs < bufferStartOffset || abs >= bufferEndOffset) {
        hexPairs.push(gapHexPair());
        ascii += gapAsciiChar();
        continue;
      }
      const byte = data[abs - bufferStartOffset]!;
      hexPairs.push(byteToHexPair(byte));
      ascii += byteToAsciiColumnChar(byte);
    }

    rows.push({
      offset: rowOffset,
      hexPairs,
      ascii,
    });
  }

  return rows;
}
