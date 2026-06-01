import { describe, expect, it } from 'vitest';

import {
  extractNumericTokens,
  instructionHelpForToken,
  lookupInstruction,
  normalizeMnemonic,
  parseAsmNumber,
} from './instructionHelp';

describe('instruction help', () => {
  it('normalizes mnemonics like the Qt service', () => {
    expect(normalizeMnemonic('  MOV ')).toBe('mov');
    expect(normalizeMnemonic('je/jz')).toBe('jejz');
    expect(normalizeMnemonic('XOR.')).toBe('xor');
  });

  it('looks up known mnemonics and rejects unknown ones', () => {
    expect(lookupInstruction('mov')?.title).toContain('MOV');
    expect(lookupInstruction('MOV')?.mnemonic).toBe('mov');
    expect(lookupInstruction('vpblendvb')).toBeNull();
    expect(lookupInstruction('')).toBeNull();
  });

  it('parses asm number literals in every supported base', () => {
    expect(parseAsmNumber('0x10')).toBe(16n);
    expect(parseAsmNumber('1Fh')).toBe(31n);
    expect(parseAsmNumber('0b1010')).toBe(10n);
    expect(parseAsmNumber('17o')).toBe(15n);
    expect(parseAsmNumber('42')).toBe(42n);
    expect(parseAsmNumber('xyz')).toBeNull();
    expect(parseAsmNumber('0x')).toBeNull();
  });

  it('extracts distinct numeric tokens from a line', () => {
    expect(extractNumericTokens('mov rax, 0x10 ; +0x10 again, 42')).toEqual(['0x10', '42']);
  });

  it('builds help with flags and multi-base number conversions', () => {
    const help = instructionHelpForToken('add', 'add rax, 0x10');
    expect(help).not.toBeNull();
    expect(help?.title).toContain('ADD');
    expect(help?.flags).toContain('ZF');
    expect(help?.numbers).toEqual([{ token: '0x10', dec: '16', oct: '20', hex: '0x10' }]);
  });

  it('returns help with empty flags for instructions that do not set flags', () => {
    const help = instructionHelpForToken('mov', 'mov rbp, rsp');
    expect(help?.flags).toEqual([]);
    expect(help?.numbers).toEqual([]);
  });

  it('returns null for unknown mnemonics', () => {
    expect(instructionHelpForToken('endbr64', 'endbr64')).toBeNull();
  });
});
