/**
 * Qt-parity INI serialization for AppPreferences. The export format mirrors
 * Cremniy's `AppSettings::exportToIni`: section per concern, lowercase keys,
 * boolean as `true`/`false`. JSON remains the wire format for the on-disk
 * preferences.json — INI is only used for human-readable export/import.
 */

import {
  DEFAULT_APP_PREFERENCES,
  normalizeDisassemblyPreferences,
  normalizeHexOptions,
  type AppPreferences,
  type DisassemblySyntaxPreference,
  type LocalePreference,
  type ThemePreference,
} from './appPreferences';

export function stringifyAppPreferencesIni(prefs: AppPreferences): string {
  const lines: string[] = [];
  lines.push('[General]');
  lines.push(`theme=${prefs.theme}`);
  lines.push(`locale=${prefs.locale}`);
  lines.push(`terminalPanelVisible=${prefs.terminalPanelVisible}`);
  lines.push(`editorWordWrap=${prefs.editorWordWrap}`);
  lines.push(`editorFontSize=${prefs.editorFontSize}`);
  lines.push(`editorInsertSpaces=${prefs.editorInsertSpaces}`);
  lines.push(`editorTabWidth=${prefs.editorTabWidth}`);
  lines.push(`excludedFilePatterns=${prefs.excludedFilePatterns.replace(/\n/g, '\\n')}`);
  lines.push('');
  lines.push('[Hex]');
  lines.push(`bytesPerLine=${prefs.hexOptions.bytesPerLine}`);
  lines.push(`addressWidth=${prefs.hexOptions.addressWidth}`);
  lines.push(`groupLength=${prefs.hexOptions.groupLength}`);
  lines.push('');
  lines.push('[Disassembly]');
  lines.push(`backend=${prefs.disassembly.backend}`);
  lines.push(`objdumpPath=${prefs.disassembly.objdumpPath}`);
  lines.push(`archHint=${prefs.disassembly.archHint}`);
  lines.push(`instructionLimit=${prefs.disassembly.instructionLimit}`);
  lines.push(`syntax=${prefs.disassembly.syntax}`);
  lines.push(`radare2Path=${prefs.disassembly.radare2Path}`);
  lines.push(`radare2AnalysisLevel=${prefs.disassembly.radare2AnalysisLevel}`);
  lines.push(`radare2PreCommands=${prefs.disassembly.radare2PreCommands.replace(/\n/g, '\\n')}`);
  lines.push('');
  lines.push('[Recent]');
  prefs.recentWorkspacePaths.forEach((path, i) => {
    lines.push(`path${i + 1}=${path}`);
  });
  return `${lines.join('\n')}\n`;
}

export function parseAppPreferencesIni(text: string): AppPreferences {
  const sections = parseIniSections(text);

  const general = sections.General ?? {};
  const hex = sections.Hex ?? {};
  const disasm = sections.Disassembly ?? {};
  const recent = sections.Recent ?? {};

  const theme: ThemePreference =
    general.theme === 'light' || general.theme === 'dark'
      ? general.theme
      : DEFAULT_APP_PREFERENCES.theme;
  const locale: LocalePreference =
    general.locale === 'en' || general.locale === 'ru'
      ? general.locale
      : DEFAULT_APP_PREFERENCES.locale;
  const terminalPanelVisible = parseBool(general.terminalPanelVisible, DEFAULT_APP_PREFERENCES.terminalPanelVisible);
  const editorWordWrap = parseBool(general.editorWordWrap, DEFAULT_APP_PREFERENCES.editorWordWrap);
  const editorFontSize = parseNumber(general.editorFontSize, DEFAULT_APP_PREFERENCES.editorFontSize, 8, 48);
  const editorInsertSpaces = parseBool(general.editorInsertSpaces, DEFAULT_APP_PREFERENCES.editorInsertSpaces);
  const parsedTabWidth = Number(general.editorTabWidth);
  const editorTabWidth: number =
    parsedTabWidth === 2 || parsedTabWidth === 4 || parsedTabWidth === 8
      ? parsedTabWidth
      : DEFAULT_APP_PREFERENCES.editorTabWidth;
  const excludedFilePatterns =
    general.excludedFilePatterns != null
      ? general.excludedFilePatterns.replace(/\\n/g, '\n')
      : DEFAULT_APP_PREFERENCES.excludedFilePatterns;

  const hexOptions = normalizeHexOptions({
    bytesPerLine: Number(hex.bytesPerLine ?? DEFAULT_APP_PREFERENCES.hexOptions.bytesPerLine),
    addressWidth: Number(hex.addressWidth ?? DEFAULT_APP_PREFERENCES.hexOptions.addressWidth),
    groupLength: Number(hex.groupLength ?? DEFAULT_APP_PREFERENCES.hexOptions.groupLength),
  });

  const syntax: DisassemblySyntaxPreference =
    disasm.syntax === 'att' || disasm.syntax === 'intel' ? disasm.syntax : 'intel';

  const disassembly = normalizeDisassemblyPreferences({
    backend: disasm.backend ?? DEFAULT_APP_PREFERENCES.disassembly.backend,
    objdumpPath: disasm.objdumpPath ?? '',
    archHint: disasm.archHint ?? '',
    instructionLimit: Number(disasm.instructionLimit ?? DEFAULT_APP_PREFERENCES.disassembly.instructionLimit),
    syntax,
    radare2Path: disasm.radare2Path ?? '',
    radare2AnalysisLevel: disasm.radare2AnalysisLevel ?? DEFAULT_APP_PREFERENCES.disassembly.radare2AnalysisLevel,
    radare2PreCommands: (disasm.radare2PreCommands ?? '').replace(/\\n/g, '\n'),
  });

  const recentWorkspacePaths: string[] = [];
  Object.keys(recent)
    .filter((k) => k.startsWith('path'))
    .sort((a, b) => Number(a.slice(4)) - Number(b.slice(4)))
    .forEach((k) => {
      const v = recent[k]?.trim();
      if (v != null && v !== '') {
        recentWorkspacePaths.push(v);
      }
    });

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
    dockLayout: null,
  };
}

function parseBool(value: string | undefined, fallback: boolean): boolean {
  if (value == null) return fallback;
  const v = value.trim().toLowerCase();
  if (v === 'true' || v === '1' || v === 'yes') return true;
  if (v === 'false' || v === '0' || v === 'no') return false;
  return fallback;
}

function parseNumber(value: string | undefined, fallback: number, min: number, max: number): number {
  if (value == null) return fallback;
  const n = Number(value);
  if (!Number.isFinite(n)) return fallback;
  return Math.max(min, Math.min(max, Math.round(n)));
}

function parseIniSections(text: string): Record<string, Record<string, string>> {
  const out: Record<string, Record<string, string>> = {};
  let current = '';
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (line === '' || line.startsWith(';') || line.startsWith('#')) {
      continue;
    }
    const sectionMatch = /^\[(.+)\]$/.exec(line);
    if (sectionMatch != null) {
      current = sectionMatch[1]!.trim();
      if (out[current] == null) {
        out[current] = {};
      }
      continue;
    }
    const eq = line.indexOf('=');
    if (eq < 0 || current === '') {
      continue;
    }
    const key = line.slice(0, eq).trim();
    const value = line.slice(eq + 1).trim();
    if (key !== '') {
      (out[current] ??= {})[key] = value;
    }
  }
  return out;
}
