export type DisassemblyCommandResult = {
  executable: string;
  args: string[];
  cwd: string;
  filePath: string;
  stdout: string;
  stderr: string;
  statusCode: number | null;
  sectionHeadersStdout: string;
  sectionHeadersStderr: string;
  sectionHeadersStatusCode: number | null;
};

export type DisassemblySyntaxOption = 'intel' | 'att';

export type DisassembleWorkspaceFile = (
  workspaceRoot: string,
  filePath: string,
  options?: {
    objdumpPath?: string | null;
    archHint?: string | null;
    syntax?: DisassemblySyntaxOption | null;
    instructionLimit?: number | null;
  },
) => Promise<DisassemblyCommandResult>;

export type DisassemblySectionHeader = {
  name: string;
  size: number;
  vaddr: number;
  fileOffset: number;
};

export type DisassemblyInstructionRow =
  | {
      kind: 'label';
      address: string;
      label: string;
      bytes: '';
      mnemonic: string;
      operands: '';
      comment: '';
      size: 0;
      fileOffset: null;
    }
  | {
      kind: 'instruction';
      address: string;
      bytes: string;
      mnemonic: string;
      operands: string;
      comment: string;
      size: number;
      fileOffset: number | null;
    };

export type DisassemblySection = {
  name: string;
  vaddr: number | null;
  fileOffset: number | null;
  size: number | null;
  hasFileMapping: boolean;
  rows: DisassemblyInstructionRow[];
};

export type DisassemblyFunctionLabel = {
  address: string;
  name: string;
};

export type DisassemblyDocument = {
  sections: DisassemblySection[];
  functions: DisassemblyFunctionLabel[];
  errors: string[];
  metadata: {
    executable: string;
    args: string[];
    filePath: string;
    statusCode: number | null;
  };
};

export type DisassemblyListingRow = {
  id: string;
  sectionName: string;
  row: DisassemblyInstructionRow;
};

export type DisassemblyListingFilters = {
  sectionName: string;
  query: string;
};

export type DisassemblyDiagnosticEntry = {
  id: string;
  label: string;
  detail: string;
};

const SECTION_HEADER_RE =
  /^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+/;
const DISASSEMBLY_SECTION_RE = /^\s*Disassembly of section\s+(\S+)\s*:/;
const FUNCTION_LABEL_RE = /^\s*([0-9a-fA-F]+)\s+<([^>]+)>:/;
const INSTRUCTION_PREFIX_RE = /^\s*([0-9a-fA-F]+):\s+(.*)$/;
const BYTE_PAIR_RE = /^[0-9a-fA-F]{2}$/;

export function parseDisassemblySectionHeaders(raw: string): Map<string, DisassemblySectionHeader> {
  const headers = new Map<string, DisassemblySectionHeader>();

  for (const line of raw.split(/\r?\n/)) {
    const match = SECTION_HEADER_RE.exec(line);
    if (match == null) {
      continue;
    }

    const [, name, sizeRaw, vaddrRaw, fileOffsetRaw] = match;
    const size = parseHexNumber(sizeRaw);
    const vaddr = parseHexNumber(vaddrRaw);
    const fileOffset = parseHexNumber(fileOffsetRaw);
    if (name == null || size == null || vaddr == null || fileOffset == null) {
      continue;
    }

    headers.set(name, { name, size, vaddr, fileOffset });
  }

  return headers;
}

export function parseDisassemblyOutput(result: DisassemblyCommandResult): DisassemblyDocument {
  const headers = parseDisassemblySectionHeaders(result.sectionHeadersStdout);
  const sections: DisassemblySection[] = [];
  const functions: DisassemblyFunctionLabel[] = [];
  const errors = collectDisassemblyErrors(result);
  let currentSection: DisassemblySection | null = null;

  for (const line of result.stdout.split(/\r?\n/)) {
    if (line.trim() === '') {
      continue;
    }

    const sectionMatch = DISASSEMBLY_SECTION_RE.exec(line);
    if (sectionMatch != null) {
      currentSection = createSection(sectionMatch[1] ?? '', headers);
      sections.push(currentSection);
      continue;
    }

    const instruction = parseInstructionLine(line, currentSection);
    if (instruction != null) {
      currentSection = ensureSection(sections, currentSection);
      currentSection.rows.push(instruction);
      continue;
    }

    const label = parseFunctionLabelLine(line);
    if (label != null) {
      currentSection = ensureSection(sections, currentSection);
      if (label.kind === 'label') {
        functions.push({ address: label.address, name: label.label });
      }
      currentSection.rows.push(label);
    }
  }

  if (sections.length === 0 && result.statusCode === 0) {
    errors.push('objdump completed but produced no disassembly sections.');
  }

  return {
    sections,
    functions,
    errors,
    metadata: {
      executable: result.executable,
      args: result.args,
      filePath: result.filePath,
      statusCode: result.statusCode,
    },
  };
}

export function filterDisassemblyRows(
  document: DisassemblyDocument,
  filters: DisassemblyListingFilters,
): DisassemblyListingRow[] {
  const query = filters.query.trim().toLocaleLowerCase();

  return flattenDisassemblyRows(document).filter((listingRow) => {
    if (filters.sectionName !== '' && listingRow.sectionName !== filters.sectionName) {
      return false;
    }
    if (query === '') {
      return true;
    }
    return searchableRowText(listingRow.row).includes(query);
  });
}

export function findDisassemblyRowByAddress(
  document: DisassemblyDocument,
  address: string,
): DisassemblyListingRow | null {
  const normalizedAddress = normalizeAddress(address);
  return (
    flattenDisassemblyRows(document).find(
      (listingRow) => normalizeAddress(listingRow.row.address) === normalizedAddress,
    ) ?? null
  );
}

export function buildDisassemblyDiagnosticLog(
  result: DisassemblyCommandResult,
  document: DisassemblyDocument,
): DisassemblyDiagnosticEntry[] {
  const entries: DisassemblyDiagnosticEntry[] = [
    {
      id: 'command',
      label: 'Command',
      detail: `${result.executable} ${result.args.join(' ')}`,
    },
    {
      id: 'disassembly-status',
      label: 'Disassembly status',
      detail: formatStatusCode(result.statusCode),
    },
    {
      id: 'section-status',
      label: 'Section header status',
      detail: formatStatusCode(result.sectionHeadersStatusCode),
    },
  ];

  appendDiagnosticDetail(entries, 'stderr', 'stderr', result.stderr);
  appendDiagnosticDetail(
    entries,
    'section-stderr',
    'Section header stderr',
    result.sectionHeadersStderr,
  );
  document.errors.forEach((error, index) => {
    entries.push({
      id: `parser-error-${index}`,
      label: 'Parser diagnostic',
      detail: error,
    });
  });

  return entries;
}

function flattenDisassemblyRows(document: DisassemblyDocument): DisassemblyListingRow[] {
  return document.sections.flatMap((section) =>
    section.rows.map((row, index) => ({
      id: `disassembly-row-${encodeRowIdPart(section.name)}-${encodeRowIdPart(row.address)}-${index}`,
      sectionName: section.name,
      row,
    })),
  );
}

function searchableRowText(row: DisassemblyInstructionRow): string {
  const fileOffset = row.fileOffset == null ? '' : row.fileOffset.toString(16).padStart(8, '0');
  return [row.address, fileOffset, row.bytes, row.mnemonic, row.operands]
    .join(' ')
    .toLocaleLowerCase();
}

function normalizeAddress(address: string): string {
  return address.trim().toLocaleLowerCase().replace(/^0+/, '') || '0';
}

function encodeRowIdPart(value: string): string {
  return value.replace(/[^a-zA-Z0-9_-]/g, '_');
}

function formatStatusCode(statusCode: number | null): string {
  return statusCode == null ? 'Process did not report an exit code.' : `Exited with code ${statusCode}.`;
}

function appendDiagnosticDetail(
  entries: DisassemblyDiagnosticEntry[],
  id: string,
  label: string,
  detail: string,
): void {
  const trimmed = detail.trim();
  if (trimmed === '') {
    return;
  }
  entries.push({ id, label, detail: trimmed });
}

function collectDisassemblyErrors(result: DisassemblyCommandResult): string[] {
  const errors: string[] = [];
  if (result.statusCode != null && result.statusCode !== 0) {
    const detail = result.stderr.trim() || `objdump exited with code ${result.statusCode}`;
    errors.push(detail);
  }
  return errors;
}

function createSection(
  name: string,
  headers: Map<string, DisassemblySectionHeader>,
): DisassemblySection {
  const header = headers.get(name);
  return {
    name,
    vaddr: header?.vaddr ?? null,
    fileOffset: header?.fileOffset ?? null,
    size: header?.size ?? null,
    hasFileMapping: header != null,
    rows: [],
  };
}

function ensureSection(
  sections: DisassemblySection[],
  currentSection: DisassemblySection | null,
): DisassemblySection {
  if (currentSection != null) {
    return currentSection;
  }
  const section: DisassemblySection = {
    name: '.text (auto)',
    vaddr: null,
    fileOffset: null,
    size: null,
    hasFileMapping: false,
    rows: [],
  };
  sections.push(section);
  return section;
}

function parseFunctionLabelLine(line: string): DisassemblyInstructionRow | null {
  const match = FUNCTION_LABEL_RE.exec(line);
  if (match == null) {
    return null;
  }
  const [, address, label] = match;
  if (address == null || label == null) {
    return null;
  }
  return {
    kind: 'label',
    address,
    label,
    bytes: '',
    mnemonic: `<${label}>`,
    operands: '',
    comment: '',
    size: 0,
    fileOffset: null,
  };
}

function parseInstructionLine(
  line: string,
  currentSection: DisassemblySection | null,
): DisassemblyInstructionRow | null {
  const match = INSTRUCTION_PREFIX_RE.exec(line);
  if (match == null) {
    return null;
  }
  const [, address, body] = match;
  if (address == null || body == null) {
    return null;
  }

  const parsedInstruction = parseInstructionBody(body);
  if (parsedInstruction == null) {
    return null;
  }
  const { bytes, mnemonic, tail } = parsedInstruction;
  const { operands, comment } = splitOperandsAndComment(tail);
  const size = bytes === '' ? 0 : bytes.split(' ').length;

  return {
    kind: 'instruction',
    address,
    bytes,
    mnemonic,
    operands,
    comment,
    size,
    fileOffset: calculateFileOffset(address, currentSection),
  };
}

function parseInstructionBody(
  body: string,
): { bytes: string; mnemonic: string; tail: string } | null {
  const bytePairs: string[] = [];
  const tokenMatches = body.matchAll(/\S+/g);
  let tailStart = 0;

  for (const match of tokenMatches) {
    const token = match[0];
    if (!BYTE_PAIR_RE.test(token)) {
      break;
    }
    bytePairs.push(token);
    tailStart = (match.index ?? 0) + token.length;
  }

  if (bytePairs.length === 0) {
    return null;
  }

  const remaining = body.slice(tailStart).trim();
  if (remaining === '') {
    return null;
  }

  const [mnemonic = '', ...tailParts] = remaining.split(/\s+/);
  if (mnemonic === '') {
    return null;
  }

  return {
    bytes: bytePairs.join(' '),
    mnemonic,
    tail: tailParts.join(' '),
  };
}

function splitOperandsAndComment(raw: string): { operands: string; comment: string } {
  const trimmed = raw.trim();
  const commentStart = trimmed.indexOf('#');
  if (commentStart < 0) {
    return { operands: trimmed, comment: '' };
  }
  return {
    operands: trimmed.slice(0, commentStart).trim(),
    comment: trimmed.slice(commentStart + 1).trim(),
  };
}

function calculateFileOffset(address: string, section: DisassemblySection | null): number | null {
  if (
    section == null ||
    section.vaddr == null ||
    section.fileOffset == null ||
    section.size == null
  ) {
    return null;
  }

  const addr = parseHexNumber(address);
  if (addr == null || addr < section.vaddr) {
    return null;
  }
  const delta = addr - section.vaddr;
  return delta < section.size ? section.fileOffset + delta : null;
}

function parseHexNumber(raw: string | undefined): number | null {
  if (raw == null || raw === '') {
    return null;
  }
  const value = Number.parseInt(raw, 16);
  return Number.isFinite(value) ? value : null;
}
