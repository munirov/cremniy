# Plan: Qt exit — desktop migration (wave 1)

**Created:** 2026-04-30  
**Orchestration:** `orch-2026-04-30-qt-exit`  
**Goal:** Fully deprecate Qt as the default desktop path; **Tauri 2 + React + TypeScript (BMFP)** is the single primary entry for new development. Rust remains for shell, native I/O, subprocess hardening — **no new Qt**.

**References:** `ai_docs/develop/ui-parity/2026-04-29-qt-ui-audit.md`, `2026-04-29-react-parity-checklist.md`; prior orchestration `.cursor/workspace/completed/orch-2026-04-29-ui-bmfp/`.

---

## Dependency graph

```
QTEX-001 (inventory)
    └── QTEX-002 (ADR)
            ├── QTEX-003 (CI/README)
            └── QTEX-004 (bridge sketch)
                    ├── QTEX-009 (subprocess contract doc) ──┐
                    └── QTEX-005 (slice: Welcome + open)      │
                            ├── QTEX-006 (IDE routing)        │
                            └── QTEX-007 (settings)           │
                                    └── QTEX-008 (menus) ─────┴── QTEX-010 (checkpoint)
```

**Parallelism:** After QTEX-002, **QTEX-003** and **QTEX-004** can run in parallel. After QTEX-004, **QTEX-009** can proceed alongside **QTEX-005** once QTEX-003 is done (005 needs both 003 and 004).

---

## Task list (this cycle, max 10)

| ID | Theme | Summary |
|----|--------|---------|
| **QTEX-001** | T1 | Consolidate **inventory table** (below + gaps): every Qt module/feature → owning BMFP layer / Tauri responsibility. |
| **QTEX-002** | T2 | **Architecture decision** short doc: single desktop entry = Tauri app under `frontend/`; Qt build explicitly **deprecated** (no ambiguity). Location: `ai_docs/develop/architecture/` or `Docs/` per repo convention. |
| **QTEX-003** | T3 | **Build/CI:** Add or switch default CI to **frontend** (`npm ci`, `npm run build`, optional `tauri build` smoke); Qt job **workflow_dispatch** or `paths`-gated **only** under `cremniy/src/**`. Root **`README.md`** default dev flow = `frontend` commands only (exact copy-paste blocks). |
| **QTEX-004** | T4 | **Native bridge sketch:** Markdown + stub Rust `invoke` handlers list — FS read/write scoped paths, native **open/save** dialogs, **app config / project** dirs — events if needed. Minimal Rust; align with BMFP `infrastructure`. |
| **QTEX-005** | T5 | **Highest-value vertical slice:** **Welcome + Open project** — `dialog.open` → domain event → transition to IDE shell with **workspace root** (parity with `WelcomeForm::OpenProject` / history persistence stub acceptable). |
| **QTEX-006** | T6 | **IDE shell routing:** State machine or router: **Welcome ↔ IDE** with workspace identity; BMFP **boundary** composition only. |
| **QTEX-007** | T7 | **Settings stub:** Persist lightweight prefs via Tauri-resolved path; **domain** DTOs + **infrastructure** store — no business rules in UI. |
| **QTEX-008** | T8 | **Menu/actions facade:** Boundary-level registry aligned with Qt **`MenuFactory`** numeric keys (`"1"`…`"6"`); stub actions — **not** full feature implementations. |
| **QTEX-009** | T9 | **Subprocess contract:** Document **radare2** / binary tooling invocation via Rust **only** — argv allowlists, cwd constraints, timeouts, **preserve existing security hardening** from prior audit (migration must not regress). No full disassembler UI port in this task. |
| **QTEX-010** | T10 | **Qt removal checkpoint:** Smallest change so **new contributors never need Qt** for default build — e.g. **`cremniy/README.md`** (or `cremniy/src/README.md`) marking legacy + optional root CMake **`BUILD_QT_APP=OFF`** default or extract note; optional `legacy-qt/` only if smaller than README+flag. |

---

## T1 — Inventory table (baseline; refine in QTEX-001)

| Qt / C++ area | Path (indicative) | Proposed ownership |
|---------------|-------------------|---------------------|
| Bootstrap, styles, icon | `main.cpp`, `.qrc`, `styles/style.qss` | **boundary** themes/assets; Tauri window chrome + CSS modules |
| Welcome launcher | `app/WelcomeWindow/*` | **boundary** pages; **domain** recent-projects/history |
| IDE shell, splitters | `app/IDEWindow/*` | **boundary** layout; **domain** workspace/session |
| File tree | `widgets/filetreeview.*` | **boundary** tree; **domain** tree model contract |
| Editor tabs | `ui/filestabwidget.*` | **boundary** tabs; **domain** open buffers |
| Terminal | `widgets/terminal/*` | **Phase 2:** xterm.js or PTY bridge via Tauri Rust |
| Menu system | `ui/MenuBar/*` | **boundary** menus + **QTEX-008** registry |
| Hex / binary | `ToolTabs/Binary/QHexView`, `FormatPages/*` | **Phase 2:** canvas/virtualized grid + parsers in domain |
| Code editor | `ToolTabs/CodeEditor/QCodeEditor` | **Phase 2:** Monaco/CodeMirror |
| Disassembler | `ToolTabs/Disassembler` | **Phase 2:** list/disasm view + **QTEX-009** Rust bridge |

---

## Risks and constraints

- **Subprocess security:** External tooling (radare2, etc.) must keep **allowlisted arguments**, restricted working directories, and resource limits when moving orchestration from Qt to Rust — **document migration checklist** in QTEX-009; implementation threads through later phases.
- **Scope creep:** Do **not** port full disassembler, hex editor, or terminal PTY in a single task — **Phase 2** owns deep parity.
- **CI cost:** Qt installs are heavy — default pipeline should skip them unless legacy paths change.

---

## Phase 2 continuation (explicitly out of this cycle’s 10 tasks)

- Hex editor + PE/ELF/MBR/RAW views parity with `ToolTabs/Binary`.
- Disassembler UI + radare2 integration beyond contract doc.
- Code editor (Monaco) parity with `QCodeEditor`.
- Terminal PTY and **View → Terminal** behavior.
- Packaging/signing parity with existing **release** workflow under `cremniy/.github/workflows/release.yml` for **Tauri** artifacts.

---

## Progress (orchestrator updates)

- ✅ QTEX-001 — Completed (inventory: `ai_docs/develop/migration/2026-04-30-qt-to-tauri-inventory.md`)
- ✅ QTEX-002 — Completed (ADR: `ai_docs/develop/architecture/ADR-001-tauri-desktop-primary.md`)
- ✅ QTEX-003 — Completed (default CI + README: `frontend/` first)
- ✅ QTEX-004 — Completed (native bridge sketch: `ai_docs/develop/architecture/tauri-native-bridge.md`; Rust commands in `frontend/src-tauri`; TS bridge under `frontend/src/infrastructure/tauri/`)
- ⏳ QTEX-005 … QTEX-010 — Pending

---

## Acceptance criteria (wave)

1. A reader can open **root README** and run **only** `frontend` commands for day-one dev.
2. Default **CI** does not require Qt unless explicitly opted in.
3. **ADR** states Tauri-first and Qt-deprecated unambiguously.
4. One **end-to-end slice** (Welcome → open folder → IDE shell) exists with **real** native dialog path via Tauri.
5. Qt tree is **labeled legacy** and/or **off by default** in build docs without blocking Tauri work.
