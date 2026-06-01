import type { DisassembleWorkspaceFile } from '@domain/disassembly/disassembly';
import { disassembleWorkspaceFile } from '@infrastructure/tauri/bridge';
import { loadPreferences } from '@infrastructure/preferences/preferencesBridge';

export const disassemblerToolService: DisassembleWorkspaceFile = async (workspaceRoot, filePath) => {
  const prefs = await loadPreferences();
  return disassembleWorkspaceFile(workspaceRoot, filePath, {
    objdumpPath: prefs.disassembly.objdumpPath,
    archHint: prefs.disassembly.archHint,
    syntax: prefs.disassembly.syntax,
    instructionLimit: prefs.disassembly.instructionLimit,
  });
};
