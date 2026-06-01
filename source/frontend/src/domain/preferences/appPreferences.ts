export type ThemePreference = 'dark' | 'light';

export type DisassemblerBackendPreference = 'objdump';
export type DisassemblySyntaxPreference = 'intel' | 'att';

export type DisassemblyPreferences = {
  backend: DisassemblerBackendPreference;
  objdumpPath: string;
  archHint: string;
  instructionLimit: number;
  syntax: DisassemblySyntaxPreference;
};

export type AppPreferences = {
  theme: ThemePreference;
  recentWorkspacePaths: string[];
  /** When false, the IDE footer terminal region is hidden (View menu parity). */
  terminalPanelVisible: boolean;
  /** Monaco / editor word wrap (View menu + preferences parity). */
  editorWordWrap: boolean;
  disassembly: DisassemblyPreferences;
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
};

export const DEFAULT_APP_PREFERENCES: AppPreferences = {
  theme: 'dark',
  recentWorkspacePaths: [],
  terminalPanelVisible: true,
  editorWordWrap: true,
  disassembly: DEFAULT_DISASSEMBLY_PREFERENCES,
};

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

export function normalizeDisassemblyPreferences(raw: unknown): DisassemblyPreferences {
  if (raw == null || typeof raw !== 'object' || Array.isArray(raw)) {
    return DEFAULT_DISASSEMBLY_PREFERENCES;
  }
  const o = raw as Record<string, unknown>;
  return {
    backend: 'objdump',
    objdumpPath: normalizeTextField(o.objdumpPath),
    archHint: normalizeTextField(o.archHint),
    instructionLimit: normalizeInstructionLimit(o.instructionLimit),
    syntax: isDisassemblySyntaxPreference(o.syntax)
      ? o.syntax
      : DEFAULT_DISASSEMBLY_PREFERENCES.syntax,
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

  const disassembly = normalizeDisassemblyPreferences(o.disassembly);

  return { theme, recentWorkspacePaths, terminalPanelVisible, editorWordWrap, disassembly };
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
      recentWorkspacePaths: prefs.recentWorkspacePaths,
      terminalPanelVisible: prefs.terminalPanelVisible,
      editorWordWrap: prefs.editorWordWrap,
      disassembly: prefs.disassembly,
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
