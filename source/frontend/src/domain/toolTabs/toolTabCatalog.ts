import type { ToolTabId } from './toolTabId';

export type ToolTabCatalogEntry = {
  id: ToolTabId;
  label: string;
  railLabel: string;
  qtPath: string;
};

export const TOOL_TAB_CATALOG: readonly ToolTabCatalogEntry[] = [
  {
    id: 'binary',
    label: 'Binary / hex',
    railLabel: 'Bin',
    qtPath: 'ToolTabs/Binary/',
  },
  {
    id: 'codeEditor',
    label: 'Code editor tool',
    railLabel: 'Code',
    qtPath: 'ToolTabs/CodeEditor/',
  },
  {
    id: 'disassembler',
    label: 'Disassembler',
    railLabel: 'Disasm',
    qtPath: 'ToolTabs/Disassembler/',
  },
] as const;

export function toolTabEntry(id: ToolTabId): ToolTabCatalogEntry {
  const found = TOOL_TAB_CATALOG.find((e) => e.id === id);
  if (found == null) {
    throw new Error(`Unknown tool tab: ${id}`);
  }
  return found;
}
