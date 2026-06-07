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

export async function readWorkspaceFileChunk(
  workspaceRoot: string,
  path: string,
  offset: number,
  length: number,
): Promise<Uint8Array> {
  const bytes = await invoke<number[]>("read_workspace_file_chunk", {
    workspaceRoot,
    path,
    offset,
    length,
  });
  return new Uint8Array(bytes);
}

export async function getWorkspaceFileSize(
  workspaceRoot: string,
  path: string,
): Promise<number> {
  return invoke<number>("get_workspace_file_size", { workspaceRoot, path });
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

export async function disassembleWorkspaceFileWithRadare2(
  workspaceRoot: string,
  filePath: string,
  options?: {
    radare2Path?: string | null;
    archHint?: string | null;
    analysisLevel?: 'none' | 'aa' | 'aaa' | null;
    preCommands?: string | null;
    syntax?: DisassemblySyntaxOption | null;
    instructionLimit?: number | null;
  },
): Promise<DisassemblyCommandResult> {
  const r2Path = options?.radare2Path?.trim() ?? '';
  const archHint = options?.archHint?.trim() ?? '';
  const pre = options?.preCommands?.trim() ?? '';
  return invoke<DisassemblyCommandResult>('disassemble_with_radare2', {
    workspaceRoot,
    filePath,
    radare2Path: r2Path === '' ? null : r2Path,
    archHint: archHint === '' ? null : archHint,
    analysisLevel: options?.analysisLevel ?? null,
    preCommands: pre === '' ? null : pre,
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

export type SearchMatch = {
  line: number;
  column: number;
  preview: string;
  matchStart: number;
  matchEnd: number;
};

export type SearchFileResult = {
  path: string;
  name: string;
  matches: SearchMatch[];
};

export type SearchResponse = {
  files: SearchFileResult[];
  totalMatches: number;
  truncated: boolean;
};

export type SearchParams = {
  matchCase: boolean;
  wholeWord: boolean;
  useRegex: boolean;
  includes: string;
  excludes: string;
  maxResults?: number;
};

export async function searchWorkspace(
  workspaceRoot: string,
  query: string,
  params: SearchParams,
): Promise<SearchResponse> {
  return invoke<SearchResponse>("search_workspace", {
    workspaceRoot,
    query,
    matchCase: params.matchCase,
    wholeWord: params.wholeWord,
    useRegex: params.useRegex,
    includes: params.includes,
    excludes: params.excludes,
    maxResults: params.maxResults ?? 0,
  });
}

/** Replace every match in one file; returns the number of replacements. */
export async function replaceInFile(
  workspaceRoot: string,
  path: string,
  query: string,
  replacement: string,
  params: { matchCase: boolean; wholeWord: boolean; useRegex: boolean },
): Promise<number> {
  return invoke<number>("replace_in_file", {
    workspaceRoot,
    path,
    query,
    replacement,
    matchCase: params.matchCase,
    wholeWord: params.wholeWord,
    useRegex: params.useRegex,
  });
}

export type GitFileStatus = {
  path: string;
  absPath: string;
  name: string;
  indexStatus: string;
  workStatus: string;
  staged: boolean;
  untracked: boolean;
  /** True for an untracked directory (git collapses it to one `path/` entry). */
  isDir: boolean;
};

export type GitStatus = {
  isRepo: boolean;
  branch: string | null;
  ahead: number;
  behind: number;
  files: GitFileStatus[];
};

/** Working-tree status of the workspace's git repo (porcelain). `isRepo` is
 *  false when the folder isn't a repo or git isn't on PATH. */
export async function gitStatus(workspaceRoot: string): Promise<GitStatus> {
  return invoke<GitStatus>("git_status", { workspaceRoot });
}

/** `git init` in the workspace folder. */
export async function gitInit(workspaceRoot: string): Promise<void> {
  return invoke<void>("git_init", { workspaceRoot });
}

/** Stage repo-relative paths (`git add`). */
export async function gitStage(workspaceRoot: string, paths: string[]): Promise<void> {
  return invoke<void>("git_stage", { workspaceRoot, paths });
}

/** Unstage repo-relative paths (`git reset HEAD`). */
export async function gitUnstage(workspaceRoot: string, paths: string[]): Promise<void> {
  return invoke<void>("git_unstage", { workspaceRoot, paths });
}

/** Commit the staged changes with the given message (no extra trailers).
 *  `amend` rewrites the last commit (keeps its message if `message` is empty). */
export async function gitCommit(
  workspaceRoot: string,
  message: string,
  amend = false,
): Promise<void> {
  return invoke<void>("git_commit", { workspaceRoot, message, amend });
}

/** Push local commits to the upstream. */
export async function gitPush(workspaceRoot: string): Promise<void> {
  return invoke<void>("git_push", { workspaceRoot });
}

/** Pull upstream commits into the working tree. */
export async function gitPull(workspaceRoot: string): Promise<void> {
  return invoke<void>("git_pull", { workspaceRoot });
}

export async function writeUserFile(path: string, contents: string): Promise<void> {
  return invoke<void>("write_user_file", { path, contents });
}

export async function createProjectFolder(parentPath: string, folderName: string): Promise<string> {
  return invoke<string>("create_project_folder", { parentPath, folderName });
}

export async function createCremniyProject(
  parentPath: string,
  folderName: string,
  metadataJson: string,
): Promise<string> {
  return invoke<string>("create_cremniy_project", {
    parentPath,
    folderName,
    metadataJson,
  });
}

export async function readCremniyMeta(workspaceRoot: string): Promise<string> {
  return invoke<string>("read_cremniy_meta", { workspaceRoot });
}

export async function writeCremniyMeta(workspaceRoot: string, metadataJson: string): Promise<void> {
  return invoke<void>("write_cremniy_meta", { workspaceRoot, metadataJson });
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

export async function revealInFileManager(path: string): Promise<void> {
  return invoke<void>("reveal_in_file_manager", { path });
}

export async function testExternalTool(
  name: string,
  path?: string | null,
  versionArg?: string | null,
): Promise<string> {
  return invoke<string>("test_external_tool", {
    name,
    path: path?.trim() ? path.trim() : null,
    versionArg: versionArg?.trim() ? versionArg.trim() : null,
  });
}

export type ShellcodeResult = {
  bytes: number[];
  stderr: string;
  nasmPath: string;
};

export async function assembleWithNasm(
  source: string,
  bits: 16 | 32 | 64,
  nasmPath?: string | null,
): Promise<ShellcodeResult> {
  return invoke<ShellcodeResult>("assemble_with_nasm", {
    source,
    bits,
    nasmPath: nasmPath?.trim() ? nasmPath.trim() : null,
  });
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

export async function resizeTerminalSession(
  sessionId: string,
  rows: number,
  cols: number,
): Promise<void> {
  return invoke<void>("resize_terminal_session", { sessionId, rows, cols });
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

// Pop-out panes (detach a docked block into its own native window).

export async function popoutPane(paneId: string, root?: string | null): Promise<string> {
  return invoke<string>("popout_pane", {
    paneId,
    root: root == null || root === "" ? null : root,
  });
}

export async function closePopoutPane(paneId: string): Promise<void> {
  await invoke<void>("close_popout_pane", { paneId });
}

export async function listPopoutPanes(): Promise<string[]> {
  return invoke<string[]>("list_popout_panes");
}

export async function listenPopoutClosed(
  onClosed: (paneId: string) => void,
): Promise<UnlistenFn> {
  return listen<string>("pane:popout-closed", (event: Event<string>) => {
    onClosed(event.payload);
  });
}

// Binary analysis (Symbol Table / Imports / Exports / Sections).

export type BinarySectionDto = {
  name: string;
  vma: string;
  size: number;
  fileOffset: number;
  isExecutable: boolean;
  isWritable: boolean;
  isReadable: boolean;
};

export type BinarySymbolDto = {
  name: string;
  address: string;
  size: number | null;
  kind: string;
  binding: string;
  source: string;
};

export type BinaryAnalysisDto = {
  format: string;
  bitness: number;
  sections: BinarySectionDto[];
  symbols: BinarySymbolDto[];
};

export async function analyzeBinary(
  workspaceRoot: string,
  filePath: string,
): Promise<BinaryAnalysisDto> {
  return invoke<BinaryAnalysisDto>("analyze_binary", { workspaceRoot, filePath });
}
