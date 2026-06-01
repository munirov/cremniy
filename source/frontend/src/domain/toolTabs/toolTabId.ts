export const TOOL_TAB_IDS = ['binary', 'codeEditor', 'disassembler'] as const;

export type ToolTabId = (typeof TOOL_TAB_IDS)[number];
