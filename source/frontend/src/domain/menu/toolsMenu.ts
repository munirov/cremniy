export type ToolsMenuActionId =
  | 'reverseCalculator'
  | 'openBinaryTool'
  | 'openDisassemblerTool'
  | 'dataConverter'
  | 'shellCodeGenerator';

export type ToolsMenuEntry = { id: ToolsMenuActionId; label: string };

export const TOOLS_MENU_ENTRIES: readonly ToolsMenuEntry[] = [
  { id: 'openBinaryTool', label: 'Binary / hex tool' },
  { id: 'openDisassemblerTool', label: 'Disassembler' },
  { id: 'reverseCalculator', label: 'Reverse Calculator' },
  { id: 'dataConverter', label: 'Data Converter' },
  { id: 'shellCodeGenerator', label: 'Shellcode Generator' },
] as const;
