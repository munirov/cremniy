import type { ToolTabId } from './toolTabId';

export type ToolTabCatalogEntry = {
  id: ToolTabId;
  label: string;
  /**
   * Inline SVG path data (`<path d="...">`) for the rail icon. Drawn on a
   * 24×24 viewBox with `stroke="currentColor" stroke-width="1.5" fill="none"`
   * so the icons render uniformly in the rail and inherit the foreground
   * color. Hand-picked Lucide-style strokes — chosen to be readable at 16px
   * and obvious at a glance:
   *   binary       — 2×2 grid of bytes
   *   disassembler — three text rows (a listing)
   *   strings      — uppercase T inside quotes
   *   symbols      — hash, indices
   *   memoryMap    — three stacked sections (heap-style layout)
   *   functions    — f-glyph in braces
   *   patches      — wrench (modify / patch)
   *   resources    — framed picture
   */
  railIconPath: string;
  qtPath: string;
};

// `codeEditor` intentionally omitted from the rail catalog — the central
// Editor pane already shows the active document.
export const TOOL_TAB_CATALOG: readonly ToolTabCatalogEntry[] = [
  {
    id: 'binary',
    label: 'Binary / hex',
    railIconPath:
      'M4 4h7v7H4zM13 4h7v7h-7zM4 13h7v7H4zM13 13h7v7h-7z',
    qtPath: 'ToolTabs/Binary/',
  },
  {
    id: 'disassembler',
    label: 'Disassembler',
    railIconPath: 'M4 7h6M4 12h12M4 17h9M14 7h6',
    qtPath: 'ToolTabs/Disassembler/',
  },
  {
    id: 'strings',
    label: 'Strings',
    railIconPath: 'M6 4h12M12 4v16M7 20h10',
    qtPath: 'ToolTabs/Strings/',
  },
  {
    id: 'symbols',
    label: 'Symbols / imports / exports',
    railIconPath: 'M9 3L7 21M17 3l-2 18M4 9h17M3 15h17',
    qtPath: 'ToolTabs/Symbols/',
  },
  {
    id: 'memoryMap',
    label: 'Memory map (sections)',
    railIconPath: 'M3 5h18v4H3zM3 11h18v4H3zM3 17h18v4H3z',
    qtPath: 'ToolTabs/MemoryMap/',
  },
  {
    id: 'functions',
    label: 'Function list',
    railIconPath:
      'M8 4c-2 0-3 1-3 4v2c0 1.5-.5 2-2 2 1.5 0 2 .5 2 2v2c0 3 1 4 3 4M16 4c2 0 3 1 3 4v2c0 1.5.5 2 2 2-1.5 0-2 .5-2 2v2c0 3-1 4-3 4M10 12h4',
    qtPath: 'ToolTabs/Functions/',
  },
  {
    id: 'patches',
    label: 'Patches',
    railIconPath:
      'M14 5l5 5l-9 9l-5 1l1-5zM12 7l5 5',
    qtPath: 'ToolTabs/Patches/',
  },
  {
    id: 'resources',
    label: 'Resources (PE)',
    railIconPath:
      'M3 5h18v14H3zM3 15l5-5l4 4l3-3l6 6M8 10a1.5 1.5 0 1 0 0-3a1.5 1.5 0 0 0 0 3z',
    qtPath: 'ToolTabs/Resources/',
  },
] as const;

export function toolTabEntry(id: ToolTabId): ToolTabCatalogEntry {
  const found = TOOL_TAB_CATALOG.find((e) => e.id === id);
  if (found == null) {
    throw new Error(`Unknown tool tab: ${id}`);
  }
  return found;
}
