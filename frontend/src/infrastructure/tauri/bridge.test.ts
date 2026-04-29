import { describe, it, expect, vi, beforeEach } from "vitest";

vi.mock("@tauri-apps/api/core", () => ({
  invoke: vi.fn(),
}));

import { invoke } from "@tauri-apps/api/core";
import {
  getAppConfigDir,
  pickFolder,
  readTextFile,
  writeAppConfig,
} from "./bridge";

describe("tauri bridge", () => {
  beforeEach(() => {
    vi.mocked(invoke).mockReset();
  });

  it("pickFolder forwards to invoke with pick_folder", async () => {
    vi.mocked(invoke).mockResolvedValueOnce("/chosen");
    await expect(pickFolder()).resolves.toBe("/chosen");
    expect(invoke).toHaveBeenCalledWith("pick_folder");
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
});
