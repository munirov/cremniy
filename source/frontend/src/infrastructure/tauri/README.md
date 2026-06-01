# Tauri IPC (infrastructure)

Thin wrappers around `@tauri-apps/api/core` `invoke` for commands registered in **`source/backend`**.

- **`bridge.ts`** — typed helpers for Rust commands (`pick_folder`, `get_app_config_dir`, `read_text_file`, `write_app_config`).

Rust implements native behavior in **`source/backend`**. Domain logic must not import from here unless going through store ports; prefer injecting test doubles instead of mocking Tauri globally in domain tests.

See `../../../../ai_docs/develop/architecture/tauri-native-bridge.md`.
