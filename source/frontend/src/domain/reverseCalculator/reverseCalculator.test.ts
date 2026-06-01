import { describe, expect, it } from 'vitest';

import {
  formatBin,
  formatHex,
  maskToWidth,
  parseNumericInput,
  swapEndian,
  toSigned,
} from './reverseCalculator';

describe('reverseCalculator', () => {
  it('parses decimal, hex, binary, and minus', () => {
    expect(parseNumericInput('42')).toEqual({ ok: true, value: 42n });
    expect(parseNumericInput('0xFF')).toEqual({ ok: true, value: 255n });
    expect(parseNumericInput('0b1010')).toEqual({ ok: true, value: 10n });
    expect(parseNumericInput('-1')).toEqual({ ok: true, value: BigInt.asUintN(64, -1n) });
  });

  it('rejects invalid input', () => {
    expect(parseNumericInput('')).toEqual({ ok: false });
    expect(parseNumericInput('not a number')).toEqual({ ok: false });
  });

  it('masks to bit width', () => {
    expect(maskToWidth(0xffffn, 8)).toBe(0xffn);
  });

  it('formats hex with width', () => {
    expect(formatHex(0xabn, 16)).toBe('0x00AB');
  });

  it('formats binary grouped by byte', () => {
    expect(formatBin(3n, 8)).toBe('00000011');
  });

  it('signed 8-bit for 0x80', () => {
    expect(toSigned(0x80n, 8)).toBe(-128n);
  });

  it('swap endian 16-bit', () => {
    expect(swapEndian(0x1122n, 16)).toBe(0x2211n);
  });
});
