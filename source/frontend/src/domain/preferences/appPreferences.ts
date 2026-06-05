export type ThemePreference = 'dark' | 'light';

export type LocalePreference = 'en' | 'ru';

export type DisassemblerBackendPreference = 'objdump' | 'radare2';
export type DisassemblySyntaxPreference = 'intel' | 'att';
export type Radare2AnalysisLevel = 'none' | 'aa' | 'aaa';

export type DisassemblyPreferences = {
  backend: DisassemblerBackendPreference;
  objdumpPath: string;
  archHint: string;
  instructionLimit: number;
  syntax: DisassemblySyntaxPreference;
  /** Path to the radare2 binary (`r2`); empty → search PATH. */
  radare2Path: string;
  /** r2 analysis level: none (no aa), aa (basic), aaa (full). */
  radare2AnalysisLevel: Radare2AnalysisLevel;
  /** Lines of r2 commands executed before any JSON query (one per line). */
  radare2PreCommands: string;
};

export type HexOptions = {
  /** How many bytes per row in the hex viewer (8 / 16 / 32). */
  bytesPerLine: number;
  /** Number of hex digits for the address column (8 / 16). */
  addressWidth: number;
  /** Bytes between visual groups inside a row (1 / 2 / 4 / 8). */
  groupLength: number;
};

export const DEFAULT_HEX_OPTIONS: HexOptions = {
  bytesPerLine: 16,
  addressWidth: 8,
  groupLength: 8,
};

export type AppPreferences = {
  theme: ThemePreference;
  /** UI language — `en` or `ru` (Qt parity LanguageManager). */
  locale: LocalePreference;
  recentWorkspacePaths: string[];
  /** When false, the IDE footer terminal region is hidden (View menu parity). */
  terminalPanelVisible: boolean;
  /** Monaco / editor word wrap (View menu + preferences parity). */
  editorWordWrap: boolean;
  /** Monaco font size — persisted across Ctrl+wheel zoom. */
  editorFontSize: number;
  /** If true, Tab key inserts spaces; if false, a real tab character. (Qt View menu parity.) */
  editorInsertSpaces: boolean;
  /** Display width of a tab character / spaces-per-indent (2 | 4 | 8). */
  editorTabWidth: number;
  /** Glob-ish path-fragment patterns the file tree should hide (one per line). */
  excludedFilePatterns: string;
  /** Hex viewer layout (row width, address width, group length). */
  hexOptions: HexOptions;
  disassembly: DisassemblyPreferences;
  /**
   * Serialized split-layout weights (see boundary/layout/IdeDockview.tsx).
   * `null` means use the default layout. Shape is `{ outer: number[]; topRow: number[] }`,
   * but stored as opaque `unknown` to survive layout-schema evolution.
   */
  dockLayout: unknown | null;
};

export const MAX_RECENT_WORKSPACES = 10;
export const MIN_DISASSEMBLY_INSTRUCTION_LIMIT = 50;
export const MAX_DISASSEMBLY_INSTRUCTION_LIMIT = 200_000;

export const DEFAULT_DISASSEMBLY_PREFERENCES: DisassemblyPreferences = {
  backend: 'objdump',
  objdumpPath: '',
  archHint: '',
  instructionLimit: 2_000,
  syntax: 'intel',
  radare2Path: '',
  radare2AnalysisLevel: 'none',
  radare2PreCommands: '',
};

export const DEFAULT_APP_PREFERENCES: AppPreferences = {
  theme: 'dark',
  locale: 'en',
  recentWorkspacePaths: [],
  terminalPanelVisible: true,
  editorWordWrap: true,
  editorFontSize: 14,
  editorInsertSpaces: false,
  editorTabWidth: 4,
  excludedFilePatterns: '.git\nnode_modules\ntarget\n.idea\n.vscode',
  hexOptions: DEFAULT_HEX_OPTIONS,
  disassembly: DEFAULT_DISASSEMBLY_PREFERENCES,
  dockLayout: null,
};

function pickFromList<T extends number>(value: unknown, allowed: readonly T[], fallback: T): T {
  if (typeof value === 'number' && (allowed as readonly number[]).includes(value)) {
    return value as T;
  }
  return fallback;
}

export function normalizeHexOptions(raw: unknown): HexOptions {
  if (raw == null || typeof raw !== 'object' || Array.isArray(raw)) {
    return DEFAULT_HEX_OPTIONS;
  }
  const o = raw as Record<string, unknown>;
  return {
    bytesPerLine: pickFromList(o.bytesPerLine, [8, 16, 32] as const, DEFAULT_HEX_OPTIONS.bytesPerLine),
    addressWidth: pickFromList(o.addressWidth, [8, 16] as const, DEFAULT_HEX_OPTIONS.addressWidth),
    groupLength: pickFromList(o.groupLength, [1, 2, 4, 8] as const, DEFAULT_HEX_OPTIONS.groupLength),
  };
}

function isThemePreference(v: unknown): v is ThemePreference {
  return v === 'dark' || v === 'light';
}

function isDisassemblySyntaxPreference(v: unknown): v is DisassemblySyntaxPreference {
  return v === 'intel' || v === 'att';
}

function normalizeTextField(value: unknown): string {
  return typeof value === 'string' ? value.trim() : '';
}

function normalizeInstructionLimit(value: unknown): number {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    return DEFAULT_DISASSEMBLY_PREFERENCES.instructionLimit;
  }
  const integerValue = Math.trunc(value);
  return Math.min(
    MAX_DISASSEMBLY_INSTRUCTION_LIMIT,
    Math.max(MIN_DISASSEMBLY_INSTRUCTION_LIMIT, integerValue),
  );
}

function isRadare2AnalysisLevel(v: unknown): v is Radare2AnalysisLevel {
  return v === 'none' || v === 'aa' || v === 'aaa';
}

export function normalizeDisassemblyPreferences(raw: unknown): DisassemblyPreferences {
  if (raw == null || typeof raw !== 'object' || Array.isArray(raw)) {
    return DEFAULT_DISASSEMBLY_PREFERENCES;
  }
  const o = raw as Record<string, unknown>;
  const backend: DisassemblerBackendPreference = o.backend === 'radare2' ? 'radare2' : 'objdump';
  return {
    backend,
    objdumpPath: normalizeTextField(o.objdumpPath),
    archHint: normalizeTextField(o.archHint),
    instructionLimit: normalizeInstructionLimit(o.instructionLimit),
    syntax: isDisassemblySyntaxPreference(o.syntax)
      ? o.syntax
      : DEFAULT_DISASSEMBLY_PREFERENCES.syntax,
    radare2Path: normalizeTextField(o.radare2Path),
    radare2AnalysisLevel: isRadare2AnalysisLevel(o.radare2AnalysisLevel)
      ? o.radare2AnalysisLevel
      : DEFAULT_DISASSEMBLY_PREFERENCES.radare2AnalysisLevel,
    radare2PreCommands: normalizeTextField(o.radare2PreCommands),
  };
}

/** Trims, drops empties, dedupes by first occurrence, caps length (Qt history parity). */
function normalizeRecentWorkspacePathsFromRaw(arr: unknown[]): string[] {
  const seen = new Set<string>();
  const out: string[] = [];
  for (const item of arr) {
    if (typeof item !== 'string') {
      continue;
    }
    const trimmed = item.trim();
    if (trimmed === '' || seen.has(trimmed)) {
      continue;
    }
    seen.add(trimmed);
    out.push(trimmed);
    if (out.length >= MAX_RECENT_WORKSPACES) {
      break;
    }
  }
  return out;
}

export function normalizeAppPreferences(parsed: unknown): AppPreferences {
  if (parsed == null || typeof parsed !== 'object' || Array.isArray(parsed)) {
    return DEFAULT_APP_PREFERENCES;
  }
  const o = parsed as Record<string, unknown>;

  let theme = DEFAULT_APP_PREFERENCES.theme;
  if (isThemePreference(o.theme)) {
    theme = o.theme;
  }

  let locale: LocalePreference = DEFAULT_APP_PREFERENCES.locale;
  if (o.locale === 'en' || o.locale === 'ru') {
    locale = o.locale;
  }

  let recentWorkspacePaths = DEFAULT_APP_PREFERENCES.recentWorkspacePaths;
  if (Array.isArray(o.recentWorkspacePaths)) {
    recentWorkspacePaths = normalizeRecentWorkspacePathsFromRaw(o.recentWorkspacePaths);
  }

  let terminalPanelVisible = DEFAULT_APP_PREFERENCES.terminalPanelVisible;
  if (typeof o.terminalPanelVisible === 'boolean') {
    terminalPanelVisible = o.terminalPanelVisible;
  }

  let editorWordWrap = DEFAULT_APP_PREFERENCES.editorWordWrap;
  if (typeof o.editorWordWrap === 'boolean') {
    editorWordWrap = o.editorWordWrap;
  }

  let editorFontSize = DEFAULT_APP_PREFERENCES.editorFontSize;
  if (typeof o.editorFontSize === 'number' && Number.isFinite(o.editorFontSize)) {
    editorFontSize = Math.max(8, Math.min(48, Math.round(o.editorFontSize)));
  }

  let editorInsertSpaces = DEFAULT_APP_PREFERENCES.editorInsertSpaces;
  if (typeof o.editorInsertSpaces === 'boolean') {
    editorInsertSpaces = o.editorInsertSpaces;
  }

  let editorTabWidth = DEFAULT_APP_PREFERENCES.editorTabWidth;
  if (o.editorTabWidth === 2 || o.editorTabWidth === 4 || o.editorTabWidth === 8) {
    editorTabWidth = o.editorTabWidth;
  }

  let excludedFilePatterns = DEFAULT_APP_PREFERENCES.excludedFilePatterns;
  if (typeof o.excludedFilePatterns === 'string') {
    excludedFilePatterns = o.excludedFilePatterns;
  }

  const disassembly = normalizeDisassemblyPreferences(o.disassembly);
  const hexOptions = normalizeHexOptions(o.hexOptions);
  const dockLayout =
    o.dockLayout != null && typeof o.dockLayout === 'object' ? (o.dockLayout as unknown) : null;

  return {
    theme,
    locale,
    recentWorkspacePaths,
    terminalPanelVisible,
    editorWordWrap,
    editorFontSize,
    editorInsertSpaces,
    editorTabWidth,
    excludedFilePatterns,
    hexOptions,
    disassembly,
    dockLayout,
  };
}

export function parseAppPreferences(json: string): AppPreferences {
  try {
    const raw: unknown = JSON.parse(json);
    return normalizeAppPreferences(raw);
  } catch {
    return DEFAULT_APP_PREFERENCES;
  }
}

export function stringifyAppPreferences(prefs: AppPreferences): string {
  return `${JSON.stringify(
    {
      theme: prefs.theme,
      locale: prefs.locale,
      recentWorkspacePaths: prefs.recentWorkspacePaths,
      terminalPanelVisible: prefs.terminalPanelVisible,
      editorWordWrap: prefs.editorWordWrap,
      editorFontSize: prefs.editorFontSize,
      editorInsertSpaces: prefs.editorInsertSpaces,
      editorTabWidth: prefs.editorTabWidth,
      excludedFilePatterns: prefs.excludedFilePatterns,
      hexOptions: prefs.hexOptions,
      disassembly: prefs.disassembly,
      dockLayout: prefs.dockLayout,
    },
    null,
    0,
  )}\n`;
}

export function withOpenedWorkspacePinned(prefs: AppPreferences, openedPath: string): AppPreferences {
  const trimmed = openedPath.trim();
  if (trimmed === '') {
    return prefs;
  }
  const rest = prefs.recentWorkspacePaths.filter((p) => p !== trimmed);
  const next = [trimmed, ...rest].slice(0, MAX_RECENT_WORKSPACES);
  return {
    ...prefs,
    recentWorkspacePaths: next,
  };
}
