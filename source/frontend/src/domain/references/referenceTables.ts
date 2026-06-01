/** Bundled offline reference rows (subset; extend as needed). */

export type AsciiRow = { dec: number; hex: string; char: string; description: string };

export const ASCII_REFERENCE_ROWS: readonly AsciiRow[] = [
  { dec: 0, hex: '00', char: 'NUL', description: 'Null' },
  { dec: 9, hex: '09', char: 'TAB', description: 'Horizontal tab' },
  { dec: 10, hex: '0A', char: 'LF', description: 'Line feed' },
  { dec: 13, hex: '0D', char: 'CR', description: 'Carriage return' },
  { dec: 27, hex: '1B', char: 'ESC', description: 'Escape' },
  { dec: 32, hex: '20', char: 'SP', description: 'Space' },
  { dec: 48, hex: '30', char: '0', description: 'Digit zero' },
  { dec: 65, hex: '41', char: 'A', description: 'Uppercase A' },
  { dec: 97, hex: '61', char: 'a', description: 'Lowercase a' },
  { dec: 127, hex: '7F', char: 'DEL', description: 'Delete' },
] as const;

export type ScancodeRow = { key: string; make: string; break: string; notes: string };

export const SCANCODE_REFERENCE_ROWS: readonly ScancodeRow[] = [
  { key: 'Esc', make: '01', break: '81', notes: 'Escape' },
  { key: '1..0', make: '02-0B', break: '82-8B', notes: 'Number row' },
  { key: 'Enter', make: '1C', break: '9C', notes: 'Main Enter' },
  { key: 'Ctrl L', make: '1D', break: '9D', notes: 'Left control' },
  { key: 'Shift L', make: '2A', break: 'AA', notes: 'Left shift' },
  { key: 'Space', make: '39', break: 'B9', notes: 'Space bar' },
  { key: 'F1', make: '3B', break: 'BB', notes: 'Function keys' },
  { key: 'Arrow Up', make: '48 E0', break: 'C8 E0', notes: 'Extended prefix' },
  { key: 'Arrow Down', make: '50 E0', break: 'D0 E0', notes: 'Extended prefix' },
] as const;
