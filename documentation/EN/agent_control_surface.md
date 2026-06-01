# Agent control surface (`window.cremniy`)

A scriptable layer over the app. The UI is driven by a person with mouse and
keyboard; `window.cremniy` lets an external client (a script, a test, a tool)
drive the **same** application logic and read the **same** on-screen state
without scraping the DOM or simulating clicks.

Each UI capability is registered as a named command, and each on-screen slice of
state is published. Commands and state producers are registered by the live
components that already own the behaviour, so the scripted path and the UI path
call the same functions and cannot drift apart.

## API

`window.cremniy` is installed in `main.tsx` before the app mounts; the rest of
the surface is registered by components as they mount.

| Member | Description |
|--------|-------------|
| `version` | Surface version (currently `1`). |
| `commands()` | `{ name, description }[]` available on the current screen. The list reflects what is mounted (Welcome vs IDE). |
| `state()` | Structured snapshot of what the user currently sees, keyed by area (`ui`, `session`, …). |
| `run(name, args?)` | Invoke a command; returns a promise with its result. Unknown names reject with a hint to call `commands()`. |

## Commands

Names are namespaced by area. `commands()` is the source of truth; the groups:

- **`welcome.*`** — recent-workspace list and project entry (Welcome screen only).
- **`file.*` / `session.*`** — open/save, tabs, active document text.
- **`view.*` / `tool.*` / `edit.*` / `dialog.*`** — IDE chrome: panels, tools, find, dialogs.
- **`fs.*`** — workspace file operations with explicit arguments (no prompts):
  `list`, `readText`, `readBytes`, `createFile`, `writeText`, `writeBytes`,
  `createFolder`, `rename`, `delete`. `writeBytes` accepts a `number[]` or a hex
  string (`"deadbeef"`).
- **`process.*`** — run programs in the workspace:
  - `run { program, args?, relativeCwd?, timeoutMs? }` — captures stdout/stderr/exit, enforces a timeout, caps output.
  - `build { source, output? }` — convenience `rustc` build.

## Examples

```js
// Navigate and inspect
await cremniy.run('welcome.openFolder');     // pick a folder, enter the IDE
cremniy.state();                              // { ui: { route: 'ide', ... } }
await cremniy.run('tool.select', { id: 'binary' });
await cremniy.run('session.openFile', { path: '/abs/path/file.bin' });
cremniy.state().session;                      // active file, open tabs, dirty flags, text

// Edit → build → run, end to end
await cremniy.run('fs.writeText', { path: 'hello.rs', text: 'fn main(){ println!("hi"); }' });
const build = await cremniy.run('process.build', { source: 'hello.rs' });  // { ok, summary, stdout, stderr }
const out = await cremniy.run('process.run', { program: './hello' });      // out.stdout === "hi\n"
```

## Process runner safety

`fs.*` and `process.*` reach the OS through Tauri IPC, so they run only in the
desktop app (`npm run tauri:dev` / `tauri:build`), not the browser preview. The
Rust runner (`source/backend/src/process.rs`) enforces:

- the working directory must resolve **inside** the workspace root;
- arguments are passed as an explicit argv — no shell, so no shell injection;
- a wall-clock timeout with a watchdog that kills the process tree;
- a per-stream output cap.

A bare program name (e.g. `hello`) that exists in the working directory is
resolved to its absolute path, so build-then-run works on every OS; PATH tools
(e.g. `cargo`) are left untouched.

## Adding a command

Register from the component or context that already owns the behaviour, so the
scripted path stays identical to the UI path:

```ts
useEffect(() => registerAgentCommands([
  { name: 'area.action', description: 'What it does { args }.', run: (a) => doThing(a) },
]), []);
```

Read the latest values through a ref inside `run`/state producers so the
registration effect can run once while still reflecting the current screen
(see `RootApp.tsx`, `IdeSessionContext.tsx`, `WelcomeView.tsx`,
`boundary/agent/AgentWorkspaceCommands.tsx`).

## Code map

| File | Role |
|------|------|
| `source/frontend/src/shared/agent/agentBridge.ts` | Registry + `window.cremniy` API. |
| `source/frontend/src/boundary/agent/AgentWorkspaceCommands.tsx` | `fs.*` / `process.*` commands. |
| `source/frontend/src/boundary/RootApp.tsx` | IDE chrome commands + `ui` state. |
| `source/frontend/src/boundary/workspace/IdeSessionContext.tsx` | `file.*` / `session.*` commands + `session` state. |
| `source/frontend/src/boundary/welcome/WelcomeView.tsx` | `welcome.*` commands. |
| `source/backend/src/process.rs` | Hardened one-shot process runner. |
