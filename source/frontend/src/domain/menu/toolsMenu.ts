export type ToolsMenuActionId =
  | 'reverseCalculator'
  | 'openBinaryTool'
  | 'openCodeEditorTool'
  | 'openDisassemblerTool';

export type ToolsMenuEntry = { id: ToolsMenuActionId; label: string };

export const TOOLS_MENU_ENTRIES: readonly ToolsMenuEntry[] = [
  { id: 'openBinaryTool', label: 'Binary / hex tool' },
  { id: 'openCodeEditorTool', label: 'Code editor tool' },
  { id: 'openDisassemblerTool', label: 'Disassembler' },
  { id: 'reverseCalculator', label: 'Reverse Calculator' },
] as const;
