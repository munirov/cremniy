import { describe, expect, it } from 'vitest';

import {
  buildDisassemblyDiagnosticLog,
  filterDisassemblyRows,
  findDisassemblyRowByAddress,
  parseDisassemblyOutput,
  parseDisassemblySectionHeaders,
  type DisassemblyCommandResult,
} from './disassembly';

const SECTION_HEADERS_FIXTURE = `
Sections:
Idx Name          Size      VMA               LMA               File off  Algn
  0 .interp       0000001c  0000000000000318  0000000000000318  00000318  2**0
 13 .text         00000121  0000000000001040  0000000000001040  00001040  2**4
 14 .fini         00000009  0000000000001164  0000000000001164  00001164  2**2
`;

const DISASSEMBLY_FIXTURE = `

app:     file format elf64-x86-64


Disassembly of section .text:

0000000000001040 <_start>:
    1040:\tf3 0f 1e fa          \tendbr64
    1044:\t31 ed                \txor    ebp,ebp
    1046:\t49 89 d1             \tmov    r9,rdx
    1049:\t5e                   \tpop    rsi
    104a:\t48 89 e2             \tmov    rdx,rsp   # stack pointer

0000000000001130 <main>:
    1130:\t55                   \tpush   rbp
    1131:\t48 89 e5             \tmov    rbp,rsp
`;

function commandResult(overrides: Partial<DisassemblyCommandResult> = {}): DisassemblyCommandResult {
  return {
    executable: 'objdump',
    args: ['-d', '-m', 'i386:x86-64', '-M', 'intel', '/w/app'],
    cwd: '/w',
    filePath: '/w/app',
    stdout: DISASSEMBLY_FIXTURE,
    stderr: '',
    statusCode: 0,
    sectionHeadersStdout: SECTION_HEADERS_FIXTURE,
    sectionHeadersStderr: '',
    sectionHeadersStatusCode: 0,
    ...overrides,
  };
}

describe('disassembly parser', () => {
  it('parses objdump section headers with file mapping metadata', () => {
    const headers = parseDisassemblySectionHeaders(SECTION_HEADERS_FIXTURE);

    expect(headers.get('.text')).toEqual({
      name: '.text',
      size: 0x121,
      vaddr: 0x1040,
      fileOffset: 0x1040,
    });
  });

  it('parses function labels from objdump output', () => {
    const doc = parseDisassemblyOutput(commandResult());

    expect(doc.functions).toEqual([
      { address: '0000000000001040', name: '_start' },
      { address: '0000000000001130', name: 'main' },
    ]);
    expect(doc.sections[0]?.rows[0]).toMatchObject({
      kind: 'label',
      address: '0000000000001040',
      label: '_start',
      mnemonic: '<_start>',
    });
  });

  it('parses instruction rows with bytes, operands, comments, and file offsets', () => {
    const doc = parseDisassemblyOutput(commandResult());
    const rows = doc.sections[0]?.rows ?? [];

    expect(rows[1]).toMatchObject({
      kind: 'instruction',
      address: '1040',
      bytes: 'f3 0f 1e fa',
      mnemonic: 'endbr64',
      operands: '',
      comment: '',
      size: 4,
      fileOffset: 0x1040,
    });
    expect(rows[5]).toMatchObject({
      kind: 'instruction',
      address: '104a',
      bytes: '48 89 e2',
      mnemonic: 'mov',
      operands: 'rdx,rsp',
      comment: 'stack pointer',
      size: 3,
      fileOffset: 0x104a,
    });
  });

  it('reports objdump process errors without discarding raw parseable output', () => {
    const doc = parseDisassemblyOutput(
      commandResult({
        stderr: 'objdump: /w/app: file format not recognized',
        statusCode: 1,
      }),
    );

    expect(doc.errors).toEqual(['objdump: /w/app: file format not recognized']);
    expect(doc.sections).toHaveLength(1);
  });

  it('does not treat auxiliary section header scan failures as disassembly errors', () => {
    const doc = parseDisassemblyOutput(
      commandResult({
        sectionHeadersStdout: '',
        sectionHeadersStderr: 'objdump: /w/fw.bin: file format not recognized',
        sectionHeadersStatusCode: 1,
      }),
    );

    expect(doc.errors).toEqual([]);
    expect(doc.sections[0]).toMatchObject({
      name: '.text',
      hasFileMapping: false,
    });
  });

  it('reports empty successful output as an actionable parser error', () => {
    const doc = parseDisassemblyOutput(
      commandResult({
        stdout: '',
        sectionHeadersStdout: '',
      }),
    );

    expect(doc.errors).toEqual(['objdump completed but produced no disassembly sections.']);
    expect(doc.sections).toEqual([]);
  });

  it('ignores odd objdump lines without losing surrounding instructions', () => {
    const doc = parseDisassemblyOutput(
      commandResult({
        stdout: `
Disassembly of section .text:
   103f:\t00                   \tadd    BYTE PTR [rax],al
   1040:\tf3 0f 1e fa          \tendbr64
\t...
   1044:\t31 ed
not an instruction row
   1046:\t49 89 d1             \tmov    r9,rdx
`,
      }),
    );

    expect(doc.errors).toEqual([]);
    expect(doc.sections[0]?.rows).toEqual([
      expect.objectContaining({
        kind: 'instruction',
        address: '103f',
        fileOffset: null,
      }),
      expect.objectContaining({
        kind: 'instruction',
        address: '1040',
        fileOffset: 0x1040,
      }),
      expect.objectContaining({
        kind: 'instruction',
        address: '1046',
        fileOffset: 0x1046,
      }),
    ]);
  });

  it('filters flattened listing rows by section and searchable instruction fields', () => {
    const doc = parseDisassemblyOutput(commandResult());

    const rows = filterDisassemblyRows(doc, { sectionName: '.text', query: 'rdx,rsp' });

    expect(rows).toEqual([
      expect.objectContaining({
        sectionName: '.text',
        row: expect.objectContaining({
          address: '104a',
          operands: 'rdx,rsp',
        }),
      }),
    ]);
  });

  it('finds rows by normalized objdump addresses', () => {
    const doc = parseDisassemblyOutput(commandResult());

    expect(findDisassemblyRowByAddress(doc, '0000000000001130')).toMatchObject({
      sectionName: '.text',
      row: {
        kind: 'label',
        address: '0000000000001130',
        label: 'main',
        bytes: '',
        mnemonic: '<main>',
        operands: '',
        comment: '',
        size: 0,
        fileOffset: null,
      },
    });
  });

  it('builds a diagnostic log from command metadata and parser diagnostics', () => {
    const result = commandResult({
      stderr: 'objdump: warning',
      statusCode: 1,
      sectionHeadersStderr: 'section scan warning',
      sectionHeadersStatusCode: 1,
    });
    const doc = parseDisassemblyOutput(result);

    expect(buildDisassemblyDiagnosticLog(result, doc)).toEqual([
      expect.objectContaining({ label: 'Command' }),
      expect.objectContaining({ label: 'Disassembly status', detail: 'Exited with code 1.' }),
      expect.objectContaining({ label: 'Section header status', detail: 'Exited with code 1.' }),
      expect.objectContaining({ label: 'stderr', detail: 'objdump: warning' }),
      expect.objectContaining({
        label: 'Section header stderr',
        detail: 'section scan warning',
      }),
      expect.objectContaining({ label: 'Parser diagnostic', detail: 'objdump: warning' }),
    ]);
  });
});
