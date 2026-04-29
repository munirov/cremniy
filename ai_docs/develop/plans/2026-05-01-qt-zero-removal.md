# Plan: Qt zero — Tauri-only desktop

**Created:** 2026-05-01  
**Orchestration:** `orch-2026-05-01-qt-zero`  
**Goal:** Remove Qt entirely from the repo (no sources, CMake, CI, or `BUILD_QT_DESKTOP`). The only in-repo desktop product is Tauri + React under `frontend/`.  
**Total tasks:** 10  
**Priority:** High

## Preconditions (planner recon)

- All `CMakeLists.txt` files found under `cremniy/src/` only (no repo-root CMakeLists in current tree).
- Workflows: `qt-legacy-ci.yml`, `release.yml` (Qt build + windeployqt), `frontend-ci.yml` (keep Tauri path).
- `BUILD_QT_DESKTOP` and Qt docs live in `README.md`, `README_ru.md`, `src/README.md`, `src/CMakeLists.txt`.
- `cremniy.desktop` exists at repo root.
- `ai_docs/develop/plans/2026-04-30-qt-exit-desktop-migration.md` and ui-parity / audit docs reference Qt/`src/` — update or supersede as needed.

## Risks

- **Loss of reference implementation** for Phase 2 UI parity: mitigate in **QTZ-002** with an annotated git tag (e.g. `pre-qt-removal-2026-05-01`), backup branch, or tarball in external storage — do not rely on repo history alone if policy requires a named artifact.
- **Release gaps:** replacing `release.yml` Qt jobs with Tauri artifacts may need secrets, cross-platform matrix tuning, and manual steps documented if CI cannot cover all targets initially.

## Dependency graph

```
QTZ-001 → QTZ-002 → QTZ-003 → QTZ-004
                  ↘ QTZ-005 → QTZ-006 → QTZ-009 → QTZ-010
                  ↘ QTZ-007 ↗
                  QTZ-003 + QTZ-005 + QTZ-007 → QTZ-008 → QTZ-009
```

## Tasks

### QTZ-001 — Inventory (T1)

- **Priority:** Critical · **Complexity:** Simple  
- **Depends:** —  
- **Actions:** Produce a definitive list: `src/` tree, every `CMakeLists.txt`, `.github/workflows/*`, `cremniy.desktop`, scripts (shell/ps1/bat/make), `README*`, `CONTRIBUTING*`, `ROADMAP*`, `ai_docs/**` (migration plan, ui-parity audit, audits), third-party readmes under former `src/` (e.g. QHexView), license notices mentioning Qt if any.  
- **Acceptance:** Checklist checked in PR description or QTZ-010; no later task discovers an unlisted Qt entry.

### QTZ-002 — Archive before deletion (T2a)

- **Priority:** High · **Complexity:** Simple  
- **Depends:** QTZ-001  
- **Actions:** Create **git tag** and/or **branch** at current `main` (or release line) documenting “last revision with Qt sources”; optionally mirror `src/` snapshot to external archive per team policy.  
- **Acceptance:** Tag/branch name recorded in QTZ-010 checklist and optional migration doc.

### QTZ-003 — Remove Qt application tree (T2b)

- **Priority:** Critical · **Complexity:** Complex  
- **Depends:** QTZ-002  
- **Actions:** Delete entire `src/` directory (preferred over stub per product direction). Do **not** replace with a stub unless policy forces a pointer file — user asked for **no Qt**.  
- **Acceptance:** No `src/` Qt/C++ sources remain; git history retains prior blobs if needed.

### QTZ-004 — CMake cleanup (T3)

- **Priority:** High · **Complexity:** Simple  
- **Depends:** QTZ-003  
- **Actions:** Confirm no remaining `CMakeLists.txt` at repo root or elsewhere for this product; remove any orphaned CMakePresets, toolchain files, or docs that only served `src/`.  
- **Acceptance:** `glob **/CMakeLists.txt` empty or only unrelated future cmake (should be none for desktop).

### QTZ-005 — CI workflows (T4)

- **Priority:** Critical · **Complexity:** Moderate  
- **Depends:** QTZ-003 (can parallel QTZ-004 after QTZ-003)  
- **Actions:** Delete `.github/workflows/qt-legacy-ci.yml`. Edit `release.yml`: remove `cmake`, `BUILD_QT_DESKTOP`, Qt setup, `windeployqt`, Qt packaging; align with Tauri release (`cargo tauri build` / artifacts from `frontend/`), reusing patterns from `frontend-ci.yml` where possible; document in workflow comments or README if any step stays manual.  
- **Acceptance:** No workflow installs Qt or builds `src/`; release pipeline produces or documents Tauri outputs only.

### QTZ-006 — Top-level contributor docs (T5)

- **Priority:** High · **Complexity:** Moderate  
- **Depends:** QTZ-005  
- **Actions:** Update `README.md`, `README_ru.md`: remove Qt install/CMake/`BUILD_QT_DESKTOP`/`qt-legacy-ci` references; single story — clone, `frontend/` dev/build. Update `CONTRIBUTING*`, `ROADMAP*` if they mention Qt or `src/`.  
- **Acceptance:** New contributor path never mentions Qt.

### QTZ-007 — Scripts and desktop file (T6)

- **Priority:** Medium · **Complexity:** Simple  
- **Depends:** QTZ-003  
- **Actions:** Grep scripts for Qt/cmake legacy paths; update or remove. Fix or remove `cremniy.desktop` — exec/icon must point to Tauri binary or be deleted with explanation in README (Linux packaging).  
- **Acceptance:** Scripts run without referencing removed paths; `.desktop` consistent or removed with docs.

### QTZ-008 — Dependency scan (T7)

- **Priority:** High · **Complexity:** Simple  
- **Depends:** QTZ-003, QTZ-005, QTZ-007  
- **Actions:** Repo-wide search: `find_package(Qt`, `Qt6::`, `windeployqt`, `BUILD_QT_DESKTOP`, `Qt6_DIR`, etc.  
- **Acceptance:** Zero matches in tracked files (allowlist only if third-party submodule removed with `src/`).

### QTZ-009 — Migration + ADR (T8)

- **Priority:** Medium · **Complexity:** Moderate  
- **Depends:** QTZ-006, QTZ-008  
- **Actions:** Update or add migration section (“Qt removed as of …”); add short **ADR addendum** under `ai_docs/develop/architecture/` (or project ADR location) stating desktop is Tauri-only and Qt sources archived per QTZ-002. Refresh ui-parity / old plan references so they state reference is tag/archive, not `src/`.  
- **Acceptance:** Docs consistent; no doc implies `src/` Qt app is still in tree.

### QTZ-010 — Verification + checklist (T9 + T10)

- **Priority:** Critical · **Complexity:** Simple  
- **Depends:** QTZ-009  
- **Actions:** In `frontend/`: `npm test` (if present), `npm run build` (and `cargo`/`tauri` as required by project scripts). Final checklist: archive tag name, CI green, grep clean, docs updated, `.desktop`/Linux notes. Optional: move orchestration to `completed` per task-management skill.  
- **Acceptance:** Commands pass locally/CI; checklist attached to completion report.

## Progress (orchestrator)

- ⏳ QTZ-001 — Inventory  
- ⏳ QTZ-002 — Archive  
- ⏳ QTZ-003 — Delete `src/`  
- ⏳ QTZ-004 — CMake cleanup  
- ⏳ QTZ-005 — Workflows  
- ⏳ QTZ-006 — README / CONTRIBUTING / ROADMAP  
- ⏳ QTZ-007 — Scripts + `.desktop`  
- ⏳ QTZ-008 — Grep scan  
- ⏳ QTZ-009 — Migration + ADR  
- ⏳ QTZ-010 — Verify + checklist  

## Execution notes

- **Parallel after QTZ-003:** QTZ-004, QTZ-005, QTZ-007 can proceed in parallel; QTZ-008 follows their completion.  
- **Subagents:** inventory/docs — general or explore; deletion + CI — worker; verification — test-runner.
