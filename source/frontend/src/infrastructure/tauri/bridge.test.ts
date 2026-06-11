import { describe, it, expect, vi, beforeEach } from "vitest";

vi.mock("@tauri-apps/api/core", () => ({
  invoke: vi.fn(),
}));
vi.mock("@tauri-apps/api/event", () => ({
  listen: vi.fn(),
}));

import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import {
  createDirectoryUnderWorkspace,
  createEmptyFileUnderWorkspace,
  createProjectFolder,
  deleteUnderWorkspace,
  disassembleWorkspaceFile,
  extractWorkspaceFileStrings,
  getAppConfigDir,
  getTerminalCapabilities,
  interruptTerminalSession,
  listenTerminalOutput,
  pickFile,
  pickFolder,
  pickSaveFile,
  readTextFile,
  readUserFile,
  readWorkspaceUserFile,
  readWorkspaceFileBytes,
  listDirectoryEntries,
  renameUnderWorkspace,
  startTerminalSession,
  stopTerminalSession,
  testObjdumpTool,
  writeTerminalInput,
  writeAppConfig,
  writeWorkspaceFileBytes,
  writeUserFile,
} from "./bridge";

describe("tauri bridge", () => {
  beforeEach(() => {
    vi.mocked(invoke).mockReset();
    vi.mocked(listen).mockReset();
  });

  it("pickFolder forwards to invoke with pick_folder", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("/chosen");
    await expect(pickFolder()).resolves.toBe("/chosen");
    expect(invoke).toHaveBeenCalledWith("pick_folder");
  });

  it("pickFile forwards to invoke with pick_file", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("/f.txt");
    await expect(pickFile()).resolves.toBe("/f.txt");
    expect(invoke).toHaveBeenCalledWith("pick_file");
  });

  it("pickSaveFile passes null defaultPath when omitted", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("/out.txt");
    await expect(pickSaveFile()).resolves.toBe("/out.txt");
    expect(invoke).toHaveBeenCalledWith("pick_save_file", { defaultPath: null });
  });

  it("pickSaveFile passes defaultPath when set", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("/out.txt");
    await expect(pickSaveFile("C:\\a\\b.txt")).resolves.toBe("/out.txt");
    expect(invoke).toHaveBeenCalledWith("pick_save_file", { defaultPath: "C:\\a\\b.txt" });
  });

  it("readUserFile passes path", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("x");
    await expect(readUserFile("/p")).resolves.toBe("x");
    expect(invoke).toHaveBeenCalledWith("read_user_file", { path: "/p" });
  });

  it("readWorkspaceUserFile forwards workspace root and path", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("file body");
    await expect(readWorkspaceUserFile("/w", "/w/a.txt")).resolves.toBe("file body");
    expect(invoke).toHaveBeenCalledWith("read_user_file_under_workspace", {
      workspaceRoot: "/w",
      path: "/w/a.txt",
    });
  });

  it("readWorkspaceFileBytes returns Uint8Array from invoke payload", async () => {
    vi.mocked(invoke).mockResolvedValueOnce([65, 66, 255]);
    const out = await readWorkspaceFileBytes("/w", "/w/a.bin");
    expect(out).toBeInstanceOf(Uint8Array);
    expect(Array.from(out)).toEqual([65, 66, 255]);
    expect(invoke).toHaveBeenCalledWith("read_workspace_file_bytes", {
      workspaceRoot: "/w",
      path: "/w/a.bin",
    });
  });

  it("readWorkspaceFileBytes propagates outside-workspace errors from invoke", async () => {
    vi.mocked(invoke).mockRejectedValueOnce("path is outside workspace");
    await expect(readWorkspaceFileBytes("/w", "/etc/passwd")).rejects.toBe("path is outside workspace");
  });

  it("readWorkspaceFileBytes propagates oversize-file errors from invoke", async () => {
    vi.mocked(invoke).mockRejectedValueOnce(
      "file exceeds maximum read size (67108864 bytes)",
    );
    await expect(readWorkspaceFileBytes("/w", "/w/huge.bin")).rejects.toMatch(/exceeds maximum read size/);
  });

  it("writeWorkspaceFileBytes forwards bytes as an invoke-safe array", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    const bytes = new Uint8Array([65, 66, 255]);

    await writeWorkspaceFileBytes("/w", "/w/a.bin", bytes);

    expect(invoke).toHaveBeenCalledWith("write_workspace_file_bytes", {
      workspaceRoot: "/w",
      path: "/w/a.bin",
      bytes: [65, 66, 255],
    });
  });

  it("extractWorkspaceFileStrings forwards min length and limit to the bounded command", async () => {
    vi.mocked(invoke).mockResolvedValueOnce([
      { offset: 0x1040, length: 5, text: "Hello" },
    ]);

    await expect(
      extractWorkspaceFileStrings("/w", "/w/a.bin", 4, 20_000),
    ).resolves.toEqual([{ offset: 0x1040, length: 5, text: "Hello" }]);

    expect(invoke).toHaveBeenCalledWith("extract_workspace_file_strings", {
      workspaceRoot: "/w",
      path: "/w/a.bin",
      minLength: 4,
      limit: 20_000,
    });
  });

  it("writeWorkspaceFileBytes snapshots bytes before invoking", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    const bytes = new Uint8Array([65, 66, 67]);

    const write = writeWorkspaceFileBytes("/w", "/w/a.bin", bytes);
    bytes[0] = 0;
    await write;

    expect(invoke).toHaveBeenCalledWith("write_workspace_file_bytes", {
      workspaceRoot: "/w",
      path: "/w/a.bin",
      bytes: [65, 66, 67],
    });
  });

  it("writeWorkspaceFileBytes propagates workspace boundary errors from invoke", async () => {
    vi.mocked(invoke).mockRejectedValueOnce("path is outside workspace");

    await expect(
      writeWorkspaceFileBytes("/w", "/etc/passwd", new Uint8Array([1])),
    ).rejects.toBe("path is outside workspace");
  });

  it("writeWorkspaceFileBytes propagates directory target errors from invoke", async () => {
    vi.mocked(invoke).mockRejectedValueOnce("path is not a regular file");

    await expect(writeWorkspaceFileBytes("/w", "/w", new Uint8Array([1]))).rejects.toBe(
      "path is not a regular file",
    );
  });

  it("writeWorkspaceFileBytes propagates oversize-write errors from invoke", async () => {
    vi.mocked(invoke).mockRejectedValueOnce(
      "file exceeds maximum write size (67108864 bytes)",
    );

    await expect(
      writeWorkspaceFileBytes("/w", "/w/huge.bin", new Uint8Array([1])),
    ).rejects.toMatch(/exceeds maximum write size/);
  });

  it("disassembleWorkspaceFile forwards workspace file and normalized arch hint", async () => {
    vi.mocked(invoke).mockResolvedValueOnce({
      executable: "objdump",
      args: ["-d", "/w/a"],
      cwd: "/w",
      filePath: "/w/a",
      stdout: "",
      stderr: "",
      statusCode: 0,
      sectionHeadersStdout: "",
      sectionHeadersStderr: "",
      sectionHeadersStatusCode: 0,
    });

    await expect(
      disassembleWorkspaceFile("/w", "/w/a", {
        archHint: "i386",
        objdumpPath: " /usr/bin/objdump ",
        syntax: "att",
        instructionLimit: 500,
      }),
    ).resolves.toMatchObject({
      executable: "objdump",
    });
    expect(invoke).toHaveBeenCalledWith("disassemble_workspace_file", {
      workspaceRoot: "/w",
      filePath: "/w/a",
      objdumpPath: "/usr/bin/objdump",
      archHint: "i386",
      syntax: "att",
      instructionLimit: 500,
    });
  });

  it("disassembleWorkspaceFile passes null arch hint when omitted", async () => {
    vi.mocked(invoke).mockResolvedValueOnce({
      executable: "objdump",
      args: [],
      cwd: "/w",
      filePath: "/w/a.bin",
      stdout: "",
      stderr: "",
      statusCode: 0,
      sectionHeadersStdout: "",
      sectionHeadersStderr: "",
      sectionHeadersStatusCode: 0,
    });

    await disassembleWorkspaceFile("/w", "/w/a.bin");
    expect(invoke).toHaveBeenCalledWith("disassemble_workspace_file", {
      workspaceRoot: "/w",
      filePath: "/w/a.bin",
      objdumpPath: null,
      archHint: null,
      syntax: null,
      instructionLimit: null,
    });
  });

  it("disassembleWorkspaceFile passes null arch hint when blank", async () => {
    vi.mocked(invoke).mockResolvedValueOnce({
      executable: "objdump",
      args: [],
      cwd: "/w",
      filePath: "/w/a.bin",
      stdout: "",
      stderr: "",
      statusCode: 0,
      sectionHeadersStdout: "",
      sectionHeadersStderr: "",
      sectionHeadersStatusCode: 0,
    });

    await disassembleWorkspaceFile("/w", "/w/a.bin", { archHint: "  ", objdumpPath: " " });
    expect(invoke).toHaveBeenCalledWith("disassemble_workspace_file", {
      workspaceRoot: "/w",
      filePath: "/w/a.bin",
      objdumpPath: null,
      archHint: null,
      syntax: null,
      instructionLimit: null,
    });
  });

  it("testObjdumpTool normalizes optional paths", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("objdump OK");

    await expect(testObjdumpTool(" /w ", " /usr/bin/objdump ")).resolves.toBe("objdump OK");
    expect(invoke).toHaveBeenCalledWith("test_objdump_tool", {
      workspaceRoot: "/w",
      objdumpPath: "/usr/bin/objdump",
    });
  });

  it("testObjdumpTool passes nulls for blank paths", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("objdump OK");

    await testObjdumpTool(" ", "");
    expect(invoke).toHaveBeenCalledWith("test_objdump_tool", {
      workspaceRoot: null,
      objdumpPath: null,
    });
  });

  it("writeUserFile passes path and contents", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await writeUserFile("/p", "hi");
    expect(invoke).toHaveBeenCalledWith("write_user_file", { path: "/p", contents: "hi" });
  });

  it("listDirectoryEntries forwards workspace and dir paths", async () => {
    vi.mocked(invoke).mockResolvedValueOnce([
      { name: "a.txt", path: "/w/a.txt", isDirectory: false },
    ]);
    await expect(listDirectoryEntries("/w", "/w")).resolves.toEqual([
      { name: "a.txt", path: "/w/a.txt", isDirectory: false },
    ]);
    expect(invoke).toHaveBeenCalledWith("list_directory", {
      workspaceRoot: "/w",
      dirPath: "/w",
    });
  });

  it("getAppConfigDir forwards to invoke", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("/home/user/.config/app");
    await expect(getAppConfigDir()).resolves.toBe("/home/user/.config/app");
    expect(invoke).toHaveBeenCalledWith("get_app_config_dir");
  });

  it("readTextFile passes relative path", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("body");
    await expect(readTextFile("prefs.json")).resolves.toBe("body");
    expect(invoke).toHaveBeenCalledWith("read_text_file", {
      relativePath: "prefs.json",
    });
  });

  it("writeAppConfig passes payload", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await writeAppConfig("prefs.json", "{}");
    expect(invoke).toHaveBeenCalledWith("write_app_config", {
      relativePath: "prefs.json",
      contents: "{}",
    });
  });

  it("createProjectFolder forwards args", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("C:\\p\\newproj");
    await expect(createProjectFolder("C:\\p", "newproj")).resolves.toBe("C:\\p\\newproj");
    expect(invoke).toHaveBeenCalledWith("create_project_folder", {
      parentPath: "C:\\p",
      folderName: "newproj",
    });
  });

  it("createEmptyFileUnderWorkspace forwards paths", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await createEmptyFileUnderWorkspace("/w", "/w/a.txt");
    expect(invoke).toHaveBeenCalledWith("create_empty_file_under_workspace", {
      workspaceRoot: "/w",
      filePath: "/w/a.txt",
    });
  });

  it("renameUnderWorkspace forwards paths", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await renameUnderWorkspace("/w", "/w/old.txt", "/w/new.txt");
    expect(invoke).toHaveBeenCalledWith("rename_under_workspace", {
      workspaceRoot: "/w",
      fromPath: "/w/old.txt",
      toPath: "/w/new.txt",
    });
  });

  it("deleteUnderWorkspace forwards path", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await deleteUnderWorkspace("/w", "/w/x.txt");
    expect(invoke).toHaveBeenCalledWith("delete_under_workspace", {
      workspaceRoot: "/w",
      path: "/w/x.txt",
    });
  });

  it("createDirectoryUnderWorkspace forwards path", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await createDirectoryUnderWorkspace("/w", "/w/sub");
    expect(invoke).toHaveBeenCalledWith("create_directory_under_workspace", {
      workspaceRoot: "/w",
      dirPath: "/w/sub",
    });
  });

  it("startTerminalSession forwards workspace root", async () => {
    vi.mocked(invoke).mockResolvedValueOnce({
      sessionId: "terminal-1",
      shell: "powershell.exe",
      cwd: "C:\\work",
      supportsInterrupt: false,
    });

    await expect(startTerminalSession("C:\\work")).resolves.toEqual({
      sessionId: "terminal-1",
      shell: "powershell.exe",
      cwd: "C:\\work",
      supportsInterrupt: false,
    });
    expect(invoke).toHaveBeenCalledWith("start_terminal_session", {
      workspaceRoot: "C:\\work",
    });
  });

  it("writeTerminalInput forwards session input", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await writeTerminalInput("terminal-1", "dir\n");
    expect(invoke).toHaveBeenCalledWith("write_terminal_input", {
      sessionId: "terminal-1",
      input: "dir\n",
    });
  });

  it("stopTerminalSession forwards session id", async () => {
    vi.mocked(invoke).mockResolvedValueOnce(undefined);
    await stopTerminalSession("terminal-1");
    expect(invoke).toHaveBeenCalledWith("stop_terminal_session", {
      sessionId: "terminal-1",
    });
  });

  it("interruptTerminalSession forwards session id for explicit unsupported errors", async () => {
    vi.mocked(invoke).mockRejectedValueOnce(
      "terminal interrupt is not supported by the current non-PTY process bridge",
    );
    await expect(interruptTerminalSession("terminal-1")).rejects.toMatch(/not supported/);
    expect(invoke).toHaveBeenCalledWith("interrupt_terminal_session", {
      sessionId: "terminal-1",
    });
  });

  it("getTerminalCapabilities forwards to invoke", async () => {
    vi.mocked(invoke).mockResolvedValueOnce({
      supportsInterrupt: false,
      reason: "The current bridge uses std::process pipes instead of a PTY.",
    });

    await expect(getTerminalCapabilities()).resolves.toEqual({
      supportsInterrupt: false,
      reason: "The current bridge uses std::process pipes instead of a PTY.",
    });
    expect(invoke).toHaveBeenCalledWith("get_terminal_capabilities");
  });

  it("listenTerminalOutput subscribes to terminal output events", async () => {
    const unlisten = vi.fn();
    vi.mocked(listen).mockImplementationOnce(async (_event, handler) => {
      handler({
        id: 1,
        event: "terminal://output",
        payload: {
          sessionId: "terminal-1",
          stream: "stdout",
          data: "hello",
        },
      });
      return unlisten;
    });
    const onOutput = vi.fn();

    await expect(listenTerminalOutput(onOutput)).resolves.toBe(unlisten);

    expect(listen).toHaveBeenCalledWith("terminal://output", expect.any(Function));
    expect(onOutput).toHaveBeenCalledWith({
      sessionId: "terminal-1",
      stream: "stdout",
      data: "hello",
    });
  });
});
