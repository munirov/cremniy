import { describe, expect, it } from 'vitest';

import type { ToolTabId } from '@domain/toolTabs/toolTabId';
import { toolTabEntry, TOOL_TAB_CATALOG } from '@domain/toolTabs/toolTabCatalog';

// `codeEditor` is intentionally omitted from the rail — the central Editor pane
// already shows the active document.
const EXPECTED_IDS: ToolTabId[] = [
  'binary',
  'disassembler',
  'strings',
  'symbols',
  'memoryMap',
  'functions',
  'patches',
  'resources',
];

describe('toolTabCatalog', () => {
  it('lists the rail tools in order', () => {
    expect(TOOL_TAB_CATALOG.map((e) => e.id)).toEqual(EXPECTED_IDS);
  });

  it('every entry has an id, a non-empty label, and a rail icon path', () => {
    for (const entry of TOOL_TAB_CATALOG) {
      expect(entry.id).toBeTruthy();
      expect(entry.label.length).toBeGreaterThan(0);
      expect(entry.railIconPath.length).toBeGreaterThan(0);
    }
  });

  it('has unique ids', () => {
    const ids = TOOL_TAB_CATALOG.map((e) => e.id);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('toolTabEntry returns the matching entry', () => {
    const entry = toolTabEntry('disassembler');
    expect(entry.id).toBe('disassembler');
    expect(entry.label).toBe('Disassembler');
  });

  it('toolTabEntry throws on an unknown id', () => {
    expect(() => toolTabEntry('nope' as ToolTabId)).toThrow(/unknown tool tab/i);
  });
});
