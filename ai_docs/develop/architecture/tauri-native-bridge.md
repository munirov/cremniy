# Tauri native bridge (planned contract)

Minimal **Rust → frontend** IPC for Welcome flow and lightweight settings parity with the legacy Qt desktop. **Subprocess / radare2 policy:** see [subprocess-tooling-rust.md](./subprocess-tooling-rust.md).

**Rust code** lives only under `frontend/src-tauri/`. **TypeScript** callers use thin wrappers in `frontend/src/infrastructure/tauri/` (BMFP **infrastructure** — no business rules). Domain and boundary layers depend on abstractions or these wrappers, not on `@tauri-apps/api` directly.

---

## Commands (Rust `invoke`)

| Command | Direction | Purpose | Status (QTEX-004) |
|---------|-----------|---------|-------------------|
| `pick_folder` | TS → Rust → OS | Native folder picker (project root). Uses `tauri_plugin_dialog::DialogExt` → `FileDialogBuilder::blocking_pick_folder()`. | **Implemented** (real dialog) |
| `get_app_config_dir` | TS → Rust | Resolved per-user app config directory (`PathResolver::app_config_dir`). | **Implemented** |
| `read_text_file` | TS → Rust | Read UTF-8 text from a path **relative to** the app config directory (see sandbox). | **Implemented** (scoped `std::fs::read_to_string`) |
| `write_app_config` | TS → Rust | Write bytes to a file **relative to** the app config directory; creates parent dirs as needed. | **Implemented** (scoped `std::fs::write`) |

Naming matches the strings passed to `@tauri-apps/api/core` `invoke`; parameters use camelCase from TS and map to snake_case Rust args.

Future commands (documented here only, **not** in QTEX-004):

| Command | Purpose |
|---------|---------|
| `open_project_file_dialog` | Optional file picker variant if Welcome needs files vs folders only |
| Workspace-scoped FS | Reads under an opened workspace root validated in Rust |

---

## Events (Rust → frontend)

None required for QTEX-004 / Welcome parity. Prefer **polling or domain state** fed by command results. If streaming or OS callbacks are added later (e.g. watcher), emit via `tauri::Emitter` and document each event name here.

| Event | Payload | Notes |
|-------|---------|------|
| — | — | No planned events yet |

---

## Security

1. **No shell from JavaScript.** Do not expose commands that concatenate user strings into `cmd`, `powershell`, `/bin/sh`, or `std::process::Command` argv from unsecured UI payloads. External tooling stays behind dedicated Rust orchestration (**QTEX-009**).
2. **Path sandbox.** `read_text_file` / `write_app_config` resolve paths **only** under the OS app config dir for this app identifier: rejects absolute paths and `..` segments; checks `starts_with` the resolved base. This is not a substitute for filesystem **scopes** on the frontend plugin API if unrestricted paths are added later — those must use separate allowlisted prefixes.
3. **Dialogs.** Folder selection returns user-chosen paths; treat as **untrusted** until validated for workspace/session use in domain (**QTEX-005**).
4. **Secrets.** Do not persist tokens in plaintext app config unless domain explicitly defines that store; prefer OS keychains in later tasks if needed.

---

## BMFP mapping

| Layer | Responsibility |
|-------|----------------|
| **`src-tauri` (Rust)** | OS dialogs (`tauri-plugin-dialog`), path resolution (`Manager::path`), scoped file I/O, future subprocess tooling. |
| **`frontend/src/infrastructure/tauri/`** | Thin `invoke` wrappers (`bridge.ts`) and mocks in tests — **only** IPC shape. |
| **Domain** | Workspace identity, prefs DTOs, validation of roots — **calls** infra, no direct `invoke`. |
| **Boundary** | Compose UI; no direct `@tauri-apps/api` imports if project convention routes through infrastructure. |

---

## Dependencies

- `tauri-plugin-dialog` — initializes `Dialog` state and exposes `DialogExt`; folder picking uses **`FileDialogBuilder::blocking_pick_folder`** (same builder surface referenced in upstream docs).

