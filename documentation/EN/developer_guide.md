# Developer guide

Cremniy is Tauri + React. UI in WebView, native part in Rust. One process, one installer.

## Layout

```
source/
├── frontend/          # BMFP web app: Vite + React + TS
└── backend/           # Tauri Rust crate: window, IPC, OS access, process runner
```

Frontend layers — [BMFP](../architecture/BMFP.md). Repository as a whole — [BMAP](../architecture/BMAP.md).

## Run / build

```
cd source/frontend
npm install
npm run tauri:dev      # dev
npm run tauri:build    # installer
```

`tauri:dev` launches the desktop app with a hot-reloading WebView. `tauri:build` produces a
platform installer (`.msi`, `.dmg`, `.AppImage`).

## Adding a feature

For a feature with data, UI, and IPC:

1. **DTO** in `domain/<feature>/` — schema/types.
2. **Tauri wrapper** in `infrastructure/tauri/<feature>.ts` — typed `invoke` calls.
3. **Service** in `domain/<feature>/<feature>.service.ts` — orchestrates wrappers and storage.
4. **UI** in `boundary/<feature>/` — components read via hooks, actions call the service.
5. **Native command** in `source/backend/src/<feature>.rs` — implements the OS-level work;
   declare in `lib.rs`.
6. **Agent command** — register via `registerAgentCommands` per [AGENT_CONTROL](../architecture/AGENT_CONTROL.md).
7. **Tests** next to the code.

## Native shell

The Rust crate is in `source/backend/`. Each area is a module: `process.rs` (hardened process
runner), `disassembly.rs`, `terminal.rs`. `lib.rs` registers Tauri commands.

When adding a native command:

- validate inputs at the boundary;
- keep business logic in a pure function inside the module;
- never expose raw paths to the UI — resolve and bound them inside the workspace.

## Tests

- Frontend: Vitest + Testing Library, next to the code.
- Backend: `cargo test`.
- Repo-level / E2E: `tests/` at repo root (optional).
