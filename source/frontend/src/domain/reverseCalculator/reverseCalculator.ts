export type BitWidth = 8 | 16 | 32 | 64;

export function maskToWidth(value: bigint, bits: number): bigint {
  if (bits >= 64) {
    return BigInt.asUintN(64, value);
  }
  const mask = (1n << BigInt(bits)) - 1n;
  return value & mask;
}

export function toSigned(value: bigint, bits: number): bigint {
  const v = maskToWidth(value, bits);
  if (bits >= 64) {
    return BigInt.asIntN(64, v);
  }
  const signBit = 1n << BigInt(bits - 1);
  const mask = (1n << BigInt(bits)) - 1n;
  const vv = v & mask;
  if ((vv & signBit) === 0n) {
    return vv;
  }
  const neg = (~vv + 1n) & mask;
  return -neg;
}

export function swapEndian(value: bigint, bits: number): bigint {
  const bytes = Math.max(1, bits / 8);
  let v = maskToWidth(value, bits);
  let out = 0n;
  for (let i = 0; i < bytes; i += 1) {
    out = (out << 8n) | (v & 0xffn);
    v >>= 8n;
  }
  return maskToWidth(out, bits);
}

export function parseNumericInput(text: string): { ok: true; value: bigint } | { ok: false } {
  const t = text.trim();
  if (t === '') {
    return { ok: false };
  }
  const re = /^\s*([+-]?)\s*(0x[0-9a-fA-F]+|0b[01]+|\d+)\s*$/;
  const m = t.match(re);
  if (m == null) {
    return { ok: false };
  }
  const sign = m[1] ?? '';
  const num = m[2] ?? '';
  let v: bigint;
  try {
    if (num.toLowerCase().startsWith('0x')) {
      v = BigInt(num);
    } else if (num.toLowerCase().startsWith('0b')) {
      v = BigInt(num);
    } else {
      v = BigInt(num);
    }
  } catch {
    return { ok: false };
  }
  if (sign === '-') {
    v = BigInt.asUintN(64, -BigInt.asIntN(64, v));
  }
  return { ok: true, value: v };
}

export function formatHex(value: bigint, bits: number): string {
  const v = maskToWidth(value, bits);
  const nyb = Math.max(1, bits / 4);
  const hex = v.toString(16).toUpperCase();
  return `0x${hex.padStart(nyb, '0')}`;
}

export function formatBin(value: bigint, bits: number): string {
  const v = maskToWidth(value, bits);
  const parts: string[] = [];
  for (let i = bits - 1; i >= 0; i -= 1) {
    parts.push(((v >> BigInt(i)) & 1n) === 1n ? '1' : '0');
    if (i % 8 === 0 && i !== 0) {
      parts.push(' ');
    }
  }
  return parts.join('');
}
