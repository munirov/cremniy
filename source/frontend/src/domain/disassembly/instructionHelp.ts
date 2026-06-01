// Offline x86 instruction help, ported from the Qt InstructionHelpService
// (+ instructions_ru.json). Returns structured data (the Qt version built HTML).

export type InstructionHelpEntry = {
  mnemonic: string;
  title: string;
  description: string;
  flags: readonly string[];
};

export type NumberConversion = {
  token: string;
  dec: string;
  oct: string;
  hex: string;
};

export type InstructionHelp = {
  title: string;
  description: string;
  /** Affected CPU flags; empty means "does not modify flags". */
  flags: readonly string[];
  /** Numeric literals found on the instruction line, in multiple bases. */
  numbers: readonly NumberConversion[];
};

// Ported verbatim from resources/data/instructions_ru.json (locale: ru).
export const INSTRUCTION_HELP_ENTRIES: readonly InstructionHelpEntry[] = [
  { mnemonic: 'mov', title: 'MOV - копирование данных', description: 'Копирует значение из источника в назначение без изменения исходного операнда.', flags: [] },
  { mnemonic: 'add', title: 'ADD - сложение', description: 'Складывает операнды и записывает результат в первый операнд.', flags: ['CF', 'OF', 'SF', 'ZF', 'AF', 'PF'] },
  { mnemonic: 'sub', title: 'SUB - вычитание', description: 'Вычитает второй операнд из первого и сохраняет результат в первом операнде.', flags: ['CF', 'OF', 'SF', 'ZF', 'AF', 'PF'] },
  { mnemonic: 'cmp', title: 'CMP - сравнение', description: 'Вычитает второй операнд из первого только для выставления флагов, результат не сохраняется.', flags: ['CF', 'OF', 'SF', 'ZF', 'AF', 'PF'] },
  { mnemonic: 'lea', title: 'LEA - загрузка эффективного адреса', description: 'Вычисляет эффективный адрес операнда памяти и записывает его в регистр.', flags: [] },
  { mnemonic: 'and', title: 'AND - побитовое И', description: 'Выполняет побитовое логическое И между операндами.', flags: ['CF=0', 'OF=0', 'SF', 'ZF', 'PF'] },
  { mnemonic: 'or', title: 'OR - побитовое ИЛИ', description: 'Выполняет побитовое логическое ИЛИ между операндами.', flags: ['CF=0', 'OF=0', 'SF', 'ZF', 'PF'] },
  { mnemonic: 'xor', title: 'XOR - побитовое исключающее ИЛИ', description: 'Выполняет XOR между операндами.', flags: ['CF=0', 'OF=0', 'SF', 'ZF', 'PF'] },
  { mnemonic: 'test', title: 'TEST - логическая проверка', description: 'Выполняет AND для выставления флагов без сохранения результата.', flags: ['CF=0', 'OF=0', 'SF', 'ZF', 'PF'] },
  { mnemonic: 'jmp', title: 'JMP - безусловный переход', description: 'Передает управление по указанному адресу без проверки флагов.', flags: [] },
  { mnemonic: 'je', title: 'JE/JZ - переход при равенстве', description: 'Переходит, если ZF=1.', flags: [] },
  { mnemonic: 'jne', title: 'JNE/JNZ - переход при неравенстве', description: 'Переходит, если ZF=0.', flags: [] },
  { mnemonic: 'call', title: 'CALL - вызов подпрограммы', description: 'Сохраняет адрес возврата и передает управление вызываемой функции.', flags: [] },
  { mnemonic: 'ret', title: 'RET - возврат из подпрограммы', description: 'Извлекает адрес возврата со стека и передает управление обратно.', flags: [] },
  { mnemonic: 'push', title: 'PUSH - поместить в стек', description: 'Уменьшает указатель стека и сохраняет операнд в стек.', flags: [] },
  { mnemonic: 'pop', title: 'POP - извлечь из стека', description: 'Считывает значение из стека и увеличивает указатель стека.', flags: [] },
  { mnemonic: 'inc', title: 'INC - инкремент', description: 'Увеличивает операнд на 1.', flags: ['OF', 'SF', 'ZF', 'AF', 'PF'] },
  { mnemonic: 'dec', title: 'DEC - декремент', description: 'Уменьшает операнд на 1.', flags: ['OF', 'SF', 'ZF', 'AF', 'PF'] },
  { mnemonic: 'imul', title: 'IMUL - знаковое умножение', description: 'Выполняет умножение со знаком.', flags: ['CF', 'OF'] },
  { mnemonic: 'idiv', title: 'IDIV - знаковое деление', description: 'Выполняет деление со знаком (делимое в регистровой паре).', flags: [] },
];

const ENTRY_BY_MNEMONIC: ReadonlyMap<string, InstructionHelpEntry> = new Map(
  INSTRUCTION_HELP_ENTRIES.map((entry) => [entry.mnemonic, entry]),
);

/** Lowercase and strip everything but a-z0-9 (matches Qt normalizeMnemonic). */
export function normalizeMnemonic(value: string): string {
  return value.trim().toLowerCase().replace(/[^a-z0-9]/g, '');
}

export function lookupInstruction(mnemonic: string): InstructionHelpEntry | null {
  const key = normalizeMnemonic(mnemonic);
  if (key === '') {
    return null;
  }
  return ENTRY_BY_MNEMONIC.get(key) ?? null;
}

const NUMERIC_TOKEN_RE = /(0x[0-9a-fA-F]+)|([0-9a-fA-F]+h\b)|(0b[01]+)|([0-7]+o\b)|(\b\d+\b)/g;

/** Collect distinct numeric literals from a line (matches Qt extractNumericTokens). */
export function extractNumericTokens(text: string): string[] {
  const out: string[] = [];
  const seen = new Set<string>();
  for (const match of text.matchAll(NUMERIC_TOKEN_RE)) {
    const token = match[0].trim();
    if (token !== '' && !seen.has(token)) {
      seen.add(token);
      out.push(token);
    }
  }
  return out;
}

/** Parse an assembly-style number literal to a BigInt, or null (matches Qt parseAsmNumber). */
export function parseAsmNumber(token: string): bigint | null {
  const s = token.trim().toLowerCase();
  try {
    if (s.startsWith('0x')) {
      return s.length > 2 ? BigInt(s) : null;
    }
    if (s.endsWith('h')) {
      const body = s.slice(0, -1);
      return body !== '' && /^[0-9a-f]+$/.test(body) ? BigInt(`0x${body}`) : null;
    }
    if (s.startsWith('0b')) {
      return s.length > 2 ? BigInt(s) : null;
    }
    if (s.endsWith('o')) {
      const body = s.slice(0, -1);
      return body !== '' && /^[0-7]+$/.test(body) ? BigInt(`0o${body}`) : null;
    }
    return /^\d+$/.test(s) ? BigInt(s) : null;
  } catch {
    return null;
  }
}

function convertNumber(token: string): NumberConversion | null {
  const value = parseAsmNumber(token);
  if (value == null) {
    return null;
  }
  return {
    token,
    dec: value.toString(10),
    oct: value.toString(8),
    hex: `0x${value.toString(16).toUpperCase()}`,
  };
}

/**
 * Build help for a mnemonic token within its line context. Returns null when
 * the mnemonic is unknown — same contract as Qt `tooltipForToken`.
 */
export function instructionHelpForToken(token: string, lineContext: string): InstructionHelp | null {
  const entry = lookupInstruction(token);
  if (entry == null) {
    return null;
  }
  const numbers = extractNumericTokens(lineContext)
    .map(convertNumber)
    .filter((n): n is NumberConversion => n != null);

  return {
    title: entry.title === '' ? entry.mnemonic.toUpperCase() : entry.title,
    description: entry.description,
    flags: entry.flags,
    numbers,
  };
}
