import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import type { Event, UnlistenFn } from "@tauri-apps/api/event";

import type {
  TerminalCapabilities,
  TerminalOutputEvent,
  TerminalSession,
} from "@domain/terminal/terminalSession";
import type {
  DisassemblyCommandResult,
  DisassemblySyntaxOption,
} from "@domain/disassembly/disassembly";
import type { WorkspaceDirectoryEntry } from "@domain/workspace/directoryEntry";
import type {
  WorkspaceProcessOptions,
  WorkspaceProcessResult,
} from "@domain/process/workspaceProcess";

const TERMINAL_OUTPUT_EVENT = "terminal://output";

export async function pickFolder(): Promise<string | null> {
  return invoke<string | null>("pick_folder");
}

export async function pickFile(): Promise<string | null> {
  return invoke<string | null>("pick_file");
}

export async function pickSaveFile(defaultPath?: string | null): Promise<string | null> {
  return invoke<string | null>("pick_save_file", {
    defaultPath: defaultPath == null || defaultPath === "" ? null : defaultPath,
  });
}

export async function readUserFile(path: string): Promise<string> {
  return invoke<string>("read_user_file", { path });
}

export async function readWorkspaceUserFile(
  workspaceRoot: string,
  path: string,
): Promise<string> {
  return invoke<string>("read_user_file_under_workspace", { workspaceRoot, path });
}

export async function readWorkspaceFileBytes(
  workspaceRoot: string,
  path: string,
): Promise<Uint8Array> {
  const bytes = await invoke<number[]>("read_workspace_file_bytes", { workspaceRoot, path });
  return new Uint8Array(bytes);
}

export async function writeWorkspaceFileBytes(
  workspaceRoot: string,
  path: string,
  bytes: Uint8Array,
): Promise<void> {
  return invoke<void>("write_workspace_file_bytes", {
    workspaceRoot,
    path,
    bytes: Array.from(bytes),
  });
}

export async function disassembleWorkspaceFile(
  workspaceRoot: string,
  filePath: string,
  options?: {
    objdumpPath?: string | null;
    archHint?: string | null;
    syntax?: DisassemblySyntaxOption | null;
    instructionLimit?: number | null;
  },
): Promise<DisassemblyCommandResult> {
  const objdumpPath = options?.objdumpPath?.trim() ?? "";
  const archHint = options?.archHint?.trim() ?? "";
  return invoke<DisassemblyCommandResult>("disassemble_workspace_file", {
    workspaceRoot,
    filePath,
    objdumpPath: objdumpPath === "" ? null : objdumpPath,
    archHint: archHint === "" ? null : archHint,
    syntax: options?.syntax ?? null,
    instructionLimit: options?.instructionLimit ?? null,
  });
}

export async function testObjdumpTool(
  workspaceRoot?: string | null,
  objdumpPath?: string | null,
): Promise<string> {
  const normalizedWorkspaceRoot = workspaceRoot?.trim() ?? "";
  const normalizedObjdumpPath = objdumpPath?.trim() ?? "";
  return invoke<string>("test_objdump_tool", {
    workspaceRoot: normalizedWorkspaceRoot === "" ? null : normalizedWorkspaceRoot,
    objdumpPath: normalizedObjdumpPath === "" ? null : normalizedObjdumpPath,
  });
}

export async function runWorkspaceCommand(
  workspaceRoot: string,
  program: string,
  options?: WorkspaceProcessOptions,
): Promise<WorkspaceProcessResult> {
  return invoke<WorkspaceProcessResult>("run_workspace_command", {
    workspaceRoot,
    program,
    args: options?.args ?? [],
    relativeCwd:
      options?.relativeCwd == null || options.relativeCwd === ""
        ? null
        : options.relativeCwd,
    timeoutMs: options?.timeoutMs ?? null,
  });
}

export async function listDirectoryEntries(
  workspaceRoot: string,
  dirPath: string,
): Promise<WorkspaceDirectoryEntry[]> {
  return invoke<WorkspaceDirectoryEntry[]>("list_directory", {
    workspaceRoot,
    dirPath,
  });
}

export async function writeUserFile(path: string, contents: string): Promise<void> {
  return invoke<void>("write_user_file", { path, contents });
}

export async function createProjectFolder(parentPath: string, folderName: string): Promise<string> {
  return invoke<string>("create_project_folder", { parentPath, folderName });
}

export async function createEmptyFileUnderWorkspace(
  workspaceRoot: string,
  filePath: string,
): Promise<void> {
  return invoke<void>("create_empty_file_under_workspace", { workspaceRoot, filePath });
}

export async function createDirectoryUnderWorkspace(
  workspaceRoot: string,
  dirPath: string,
): Promise<void> {
  return invoke<void>("create_directory_under_workspace", { workspaceRoot, dirPath });
}

export async function renameUnderWorkspace(
  workspaceRoot: string,
  fromPath: string,
  toPath: string,
): Promise<void> {
  return invoke<void>("rename_under_workspace", { workspaceRoot, fromPath, toPath });
}

export async function deleteUnderWorkspace(workspaceRoot: string, path: string): Promise<void> {
  return invoke<void>("delete_under_workspace", { workspaceRoot, path });
}

export async function getAppConfigDir(): Promise<string> {
  return invoke<string>("get_app_config_dir");
}

export async function readTextFile(relativePath: string): Promise<string> {
  return invoke<string>("read_text_file", { relativePath });
}

export async function writeAppConfig(
  relativePath: string,
  contents: string,
): Promise<void> {
  return invoke<void>("write_app_config", { relativePath, contents });
}

export async function startTerminalSession(workspaceRoot: string): Promise<TerminalSession> {
  return invoke<TerminalSession>("start_terminal_session", { workspaceRoot });
}

export async function writeTerminalInput(sessionId: string, input: string): Promise<void> {
  return invoke<void>("write_terminal_input", { sessionId, input });
}

export async function stopTerminalSession(sessionId: string): Promise<void> {
  return invoke<void>("stop_terminal_session", { sessionId });
}

export async function interruptTerminalSession(sessionId: string): Promise<void> {
  return invoke<void>("interrupt_terminal_session", { sessionId });
}

export async function getTerminalCapabilities(): Promise<TerminalCapabilities> {
  return invoke<TerminalCapabilities>("get_terminal_capabilities");
}

export async function listenTerminalOutput(
  onOutput: (payload: TerminalOutputEvent) => void,
): Promise<UnlistenFn> {
  return listen<TerminalOutputEvent>(TERMINAL_OUTPUT_EVENT, (event: Event<TerminalOutputEvent>) => {
    onOutput(event.payload);
  });
}
