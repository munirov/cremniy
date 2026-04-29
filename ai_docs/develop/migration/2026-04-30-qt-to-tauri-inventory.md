# Qt → Tauri + React inventory (2026-04-30)

> **Historical:** This document describes the **`src/`** tree **before** it was deleted from the repository (2026-05-01). Paths are not valid on `main` after removal; use git tag **`pre-qt-removal-2026-05-01`** to inspect the old files. For the removal record see [2026-05-01-qt-zero-inventory.md](./2026-05-01-qt-zero-inventory.md) and [ADR-002](../architecture/ADR-002-qt-sources-removed.md).

## Executive summary

**Goal:** Treat the legacy **Qt 6 / C++** desktop app as deprecated for new work. **Tauri 2 + React + TypeScript (BMFP)** is the primary desktop surface; **Rust** owns the native shell (window, filesystem scoped I/O, dialogs, subprocess policy), matching project layering (`boundary` → UI composition; `domain` → contracts and business meaning; `infrastructure` → Tauri invoke and persistence).

This document maps verified paths under `cremniy/src/` (repository layout as of inventory date) to BMFP layers, native responsibilities, and migration phase (**wave1** = current orchestration slice; **phase2** = deep tool parity). Subprocess tooling must use Rust only: `../architecture/subprocess-tooling-rust.md`.

---

## Ownership matrix (Qt/C++ → BMFP / Tauri / Rust)

Paths are **verified** against the tree and `cremniy/src/CMakeLists.txt` (not inferred).

| Qt/C++ area | Repo paths (verified) | BMFP layer | Tauri / Rust responsibility | Phase | Notes / gaps |
|-------------|----------------------|------------|-----------------------------|-------|----------------|
| Application entry | `cremniy/src/main.cpp` | boundary (bootstrap) | Process entry via Tauri; single-window WebView | wave1 | Qt `QApplication` / style load replaced by Tauri + web assets |
| Welcome launcher | `cremniy/src/app/WelcomeWindow/welcomeform.cpp`, `welcomeform.h` | boundary + domain | Native folder picker via plugin; recent projects in domain + infra store | wave1 | Align with `WelcomeView` / open-project slice (QTEX-005) |
| IDE shell | `cremniy/src/app/IDEWindow/idewindow.cpp`, `idewindow.h` | boundary + domain | Window chrome optional in Rust; workspace/session state in domain | wave1 | Splitters / docks → React layout (`IdeWorkspace`) |
| File tree | `cremniy/src/widgets/filetreeview.cpp`, `filetreeview.h` | boundary (+ domain model for nodes) | Scoped directory listing via Tauri commands | wave1 | React tree still stub in parity checklist |
| Editor / file tabs | `cremniy/src/widgets/filetab.cpp`, `filetab.h`; `cremniy/src/ui/filestabwidget.cpp`, `filestabwidget.h` | boundary | Tab strip + buffer IDs in domain | wave1 | Closable/drag parity phase2+ |
| Tool tabs chrome | `cremniy/src/ui/toolstabwidget.cpp`, `toolstabwidget.h`; `cremniy/src/widgets/verticaltabwidget.h` (header-only widget), `verticaltabstyle.h`, `verticaltabstyle.cpp` | boundary | Host region for tool routes | wave1 | IDE central stack composition |
| Terminal panel | `cremniy/src/widgets/terminal/terminalwidget.cpp`, `terminalwidget.h` | boundary | PTY session in Rust; optional xterm.js front | phase2 | Heavy parity item; security-sensitive |
| Dialogs | `cremniy/src/dialogs/filecreatedialog.*`, `settingsdialog.*`, `reversecalculatordialog.*` | boundary | `dialog.open`, `dialog.save`, settings path resolution | wave1 | Settings persistence expands in QTEX-007 |
| Shared widgets | `cremniy/src/widgets/clickablelineedit.cpp`, `clickablelineedit.h` | boundary | Equivalent controls in React | wave1 | Minor UX glue |
| Menu bar | `cremniy/src/ui/MenuBar/menubarbuilder.*`, `menufactory.*`, `basemenu.h`; `cremniy/src/ui/MenuBar/Menus/File/*`, `Edit/*`, `View/*`, `Build/*`, `Tools/*`, `References/*` | boundary | Action registry + shortcuts (QTEX-008); no logic in MenuBar | wave1 | Numeric menu keys `"1"`–`"6"` preserved as contract |
| Core tab contracts | `cremniy/src/core/ToolTab.h`, `ToolTabFactory.*`, `FileDataBuffer.*` | domain | TS DTOs + factories mapping tool kinds | wave1 | Maps to BMFP domain types, not raw Qt |
| Utilities | `cremniy/src/utils/utils.*`, `appsettings.*`, `instructionhelpservice.*`, `filecontext.*`, `filemanager.*`, `projectshistorymanager.*`, `iconprovider.*` | domain + infrastructure | Paths, prefs store, history via Tauri-backed storage | wave1 | Instruction JSON stays data-driven |
| Resources | `cremniy/src/resources/cremniy_res.qrc`; `cremniy/src/resources/styles/style.qss`; `cremniy/src/resources/data/instructions_ru.json`; icons under `cremniy/src/resources/icons/` | boundary + shared | Bundled assets (`frontend/public`, `src-tauri/icons`) | wave1 | QSS → CSS Modules / tokens |
| ToolTabs — Binary | `cremniy/src/ToolTabs/Binary/binarytab.*`, `formatpage.*`, `formatpagefactory.*`; `cremniy/src/ToolTabs/Binary/FormatPages/{ELF,MBR,PE,RAW}/`; `cremniy/src/ToolTabs/Binary/QHexView/**` | boundary + domain | Large-file read policy + parsers in domain; heavy UI in React later | phase2 | CMake: `cremniy/src/ToolTabs/Binary/CMakeLists.txt` + FormatPages + QHexView |
| ToolTabs — Code editor | `cremniy/src/ToolTabs/CodeEditor/codeeditortab.*`; `cremniy/src/ToolTabs/CodeEditor/QCodeEditor/**` | boundary + domain | Monaco/CodeMirror + language defs | phase2 | CMake: `cremniy/src/ToolTabs/CodeEditor/CMakeLists.txt` |
| ToolTabs — Disassembler | `cremniy/src/ToolTabs/Disassembler/disassemblertab.*`, `disassemblerworker.*`, `disasm/**`, `disasm/backends/radare2backend.*` | boundary + domain | Subprocess + streaming contract in Rust only (QTEX-009) | phase2 | UI port follows contract doc |
| CMake / Qt build entry | `cremniy/src/CMakeLists.txt`; `cremniy/src/ToolTabs/Binary/CMakeLists.txt`; `cremniy/src/ToolTabs/CodeEditor/CMakeLists.txt`; `cremniy/src/ToolTabs/CodeEditor/QCodeEditor/CMakeLists.txt`; `cremniy/src/ToolTabs/Binary/QHexView/CMakeLists.txt`; `cremniy/src/ToolTabs/Binary/FormatPages/*/CMakeLists.txt`; `cremniy/src/ToolTabs/Disassembler/CMakeLists.txt` | — | Cargo workspace / `tauri build` as default path | wave1 | QTEX-003 / QTEX-010 deprecate Qt build for newcomers |

---

## Not in scope (wave1)

- Full **hex editor** parity (`QHexView`, binary format pages PE/ELF/MBR/RAW).
- **Code editor** parity (`QCodeEditor`, syntax styles, completers).
- **Disassembler** UI and radare2 UX beyond documented subprocess contract.
- **Terminal PTY** behavior (show/hide, scrollback, shell integration).
- **Release signing / packaging** parity with legacy Qt `release.yml` for Tauri artifacts (tracked as phase2 / separate CI work).
- CMake `target_include_directories` references `cremniy/src/terminal/ptyqt/core` while only `cremniy/src/widgets/terminal/*` is present in-tree — verify submodule or stale include when touching terminal migration.

---

## References

- `ai_docs/develop/ui-parity/2026-04-29-react-parity-checklist.md`
- `.cursor/workspace/active/orch-2026-04-30-qt-exit/plan.md`

---

Structural smoke test (required headings only; does not validate paths): from repo root run `node scripts/verify-migration-inventory.mjs`
