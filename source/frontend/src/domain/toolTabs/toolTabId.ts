export const TOOL_TAB_IDS = [
  'binary',
  'codeEditor',
  'disassembler',
  'strings',
  'symbols',
  'memoryMap',
  'functions',
  'patches',
  'resources',
] as const;

export type ToolTabId = (typeof TOOL_TAB_IDS)[number];
