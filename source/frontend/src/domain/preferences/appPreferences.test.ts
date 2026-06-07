import { describe, expect, it } from 'vitest';

import {
  normalizeAppPreferences,
  parseAppPreferences,
  stringifyAppPreferences,
  withOpenedWorkspacePinned,
  DEFAULT_APP_PREFERENCES,
  MAX_DISASSEMBLY_INSTRUCTION_LIMIT,
  MAX_RECENT_WORKSPACES,
  MIN_DISASSEMBLY_INSTRUCTION_LIMIT,
} from './appPreferences';

describe('appPreferences', () => {
  it('normalizes malformed input to defaults', () => {
    expect(normalizeAppPreferences(undefined)).toEqual(DEFAULT_APP_PREFERENCES);
    expect(normalizeAppPreferences([])).toEqual(DEFAULT_APP_PREFERENCES);
  });

  it('parseAppPreferences survives invalid JSON', () => {
    expect(parseAppPreferences('not-json')).toEqual(DEFAULT_APP_PREFERENCES);
  });

  it('roundtrips known preferences via JSON', () => {
    const prefs = {
      theme: 'light' as const,
      locale: 'ru' as const,
      recentWorkspacePaths: ['C:\\a', 'D:\\b'],
      terminalPanelVisible: false,
      editorWordWrap: false,
      editorFontSize: 18,
      editorInsertSpaces: true,
      editorTabWidth: 2 as const,
      excludedFilePatterns: '.git\nnode_modules',
      hexOptions: {
        bytesPerLine: 32,
        addressWidth: 16,
        groupLength: 4,
      },
      disassembly: {
        backend: 'objdump' as const,
        objdumpPath: 'C:\\tools\\objdump.exe',
        archHint: 'i386',
        instructionLimit: 5000,
        syntax: 'att' as const,
        radare2Path: 'C:\\tools\\r2.exe',
        radare2AnalysisLevel: 'aaa' as const,
        radare2PreCommands: 'e asm.bytes=false\ne scr.color=0',
      },
      dockLayout: null,
    };
    const again = parseAppPreferences(stringifyAppPreferences(prefs));
    expect(again.theme).toBe(prefs.theme);
    expect(again.locale).toBe(prefs.locale);
    expect(again.recentWorkspacePaths).toEqual(prefs.recentWorkspacePaths);
    expect(again.terminalPanelVisible).toBe(false);
    expect(again.editorWordWrap).toBe(false);
    expect(again.editorFontSize).toBe(18);
    expect(again.editorInsertSpaces).toBe(true);
    expect(again.editorTabWidth).toBe(2);
    expect(again.excludedFilePatterns).toBe(prefs.excludedFilePatterns);
    expect(again.hexOptions).toEqual(prefs.hexOptions);
    expect(again.disassembly).toEqual(prefs.disassembly);
  });

  it('normalizes disassembly preferences with safe defaults and bounds', () => {
    expect(normalizeAppPreferences({}).disassembly).toEqual(DEFAULT_APP_PREFERENCES.disassembly);
    expect(
      normalizeAppPreferences({
        disassembly: {
          backend: 'radare2',
          objdumpPath: '  /usr/bin/objdump  ',
          archHint: ' i386:x86-64 ',
          instructionLimit: 1,
          syntax: 'unknown',
          radare2Path: '  /usr/bin/r2  ',
          radare2AnalysisLevel: 'aaa',
          radare2PreCommands: '  e asm.bytes=false  ',
        },
      }).disassembly,
    ).toEqual({
      backend: 'radare2',
      objdumpPath: '/usr/bin/objdump',
      archHint: 'i386:x86-64',
      instructionLimit: 50,
      syntax: 'intel',
      radare2Path: '/usr/bin/r2',
      radare2AnalysisLevel: 'aaa',
      radare2PreCommands: 'e asm.bytes=false',
    });
  });

  it('clamps and truncates disassembly instruction limits', () => {
    expect(
      normalizeAppPreferences({
        disassembly: { instructionLimit: MIN_DISASSEMBLY_INSTRUCTION_LIMIT - 1 },
      }).disassembly.instructionLimit,
    ).toBe(MIN_DISASSEMBLY_INSTRUCTION_LIMIT);
    expect(
      normalizeAppPreferences({
        disassembly: { instructionLimit: MAX_DISASSEMBLY_INSTRUCTION_LIMIT + 1 },
      }).disassembly.instructionLimit,
    ).toBe(MAX_DISASSEMBLY_INSTRUCTION_LIMIT);
    expect(
      normalizeAppPreferences({
        disassembly: { instructionLimit: 123.9 },
      }).disassembly.instructionLimit,
    ).toBe(123);
  });

  it('preserves radare2 as the active backend when selected', () => {
    expect(
      normalizeAppPreferences({
        disassembly: {
          backend: 'radare2',
          objdumpPath: '/usr/bin/objdump',
        },
      }).disassembly.backend,
    ).toBe('radare2');
  });

  it('dedupes, trims, and caps recentWorkspacePaths when normalizing', () => {
    const wide = [...Array(MAX_RECENT_WORKSPACES + 5)].map((_, i) => ` /slot${i} `);
    const p = normalizeAppPreferences({
      theme: 'dark',
      recentWorkspacePaths: [' /dup ', '  /dup  ', '/keep', '', 99 as unknown as string, '/keep', ...wide],
      terminalPanelVisible: true,
    });
    expect(p.recentWorkspacePaths[0]).toBe('/dup');
    expect(p.recentWorkspacePaths[1]).toBe('/keep');
    expect(p.recentWorkspacePaths.length).toBe(MAX_RECENT_WORKSPACES);
    expect(new Set(p.recentWorkspacePaths).size).toBe(p.recentWorkspacePaths.length);
  });

  it('withOpenedWorkspacePinned dedupes, pins most recent, and caps list', () => {
    let p = DEFAULT_APP_PREFERENCES;
    p = withOpenedWorkspacePinned(p, '/first');
    p = withOpenedWorkspacePinned(p, '/second');
    p = withOpenedWorkspacePinned(p, '/first');

    expect(p.recentWorkspacePaths[0]).toBe('/first');
    expect(new Set(p.recentWorkspacePaths).size).toBe(p.recentWorkspacePaths.length);

    const manyPaths = [...Array(MAX_RECENT_WORKSPACES + 6)].map((_, i) => `/p${i}`);
    let wide = DEFAULT_APP_PREFERENCES;
    for (const path of manyPaths) {
      wide = withOpenedWorkspacePinned(wide, path);
    }
    expect(wide.recentWorkspacePaths.length).toBe(MAX_RECENT_WORKSPACES);
  });
});
