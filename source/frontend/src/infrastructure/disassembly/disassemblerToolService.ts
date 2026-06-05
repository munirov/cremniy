import type { DisassembleWorkspaceFile } from '@domain/disassembly/disassembly';
import {
  disassembleWorkspaceFile,
  disassembleWorkspaceFileWithRadare2,
} from '@infrastructure/tauri/bridge';
import { loadPreferences } from '@infrastructure/preferences/preferencesBridge';

/**
 * Routes the disassembly request to the backend selected in Settings.
 * `objdump` → embedded iced-x86 + goblin (default, no external deps).
 * `radare2` → shells out to the external `r2` binary configured in Settings.
 */
export const disassemblerToolService: DisassembleWorkspaceFile = async (
  workspaceRoot,
  filePath,
) => {
  const prefs = await loadPreferences();
  if (prefs.disassembly.backend === 'radare2') {
    return disassembleWorkspaceFileWithRadare2(workspaceRoot, filePath, {
      radare2Path: prefs.disassembly.radare2Path,
      archHint: prefs.disassembly.archHint,
      analysisLevel: prefs.disassembly.radare2AnalysisLevel,
      preCommands: prefs.disassembly.radare2PreCommands,
      syntax: prefs.disassembly.syntax,
      instructionLimit: prefs.disassembly.instructionLimit,
    });
  }
  return disassembleWorkspaceFile(workspaceRoot, filePath, {
    objdumpPath: prefs.disassembly.objdumpPath,
    archHint: prefs.disassembly.archHint,
    syntax: prefs.disassembly.syntax,
    instructionLimit: prefs.disassembly.instructionLimit,
  });
};
