import { invoke } from "@tauri-apps/api/core";

export async function pickFolder(): Promise<string | null> {
  return invoke<string | null>("pick_folder");
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
