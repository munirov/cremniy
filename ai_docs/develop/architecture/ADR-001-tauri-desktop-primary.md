# ADR-001: Tauri + React as the single primary desktop stack

| Field | Value |
|--------|--------|
| **Status** | Accepted |
| **Date** | 2026-04-30 |

---

## Context

The project previously shipped a **Qt 6 / C++** desktop application under **`src/`** (that tree has been **removed** from the repository; see [ADR-002](./ADR-002-qt-sources-removed.md)). Stakeholders **reject Qt as a desktop platform** for this product going forward.

The intended replacement is **Tauri 2 + React + TypeScript**, organized per **BMFP** (`boundary` / `domain` / `infrastructure`), with **Rust** confined to the shell (window, scoped filesystem I/O, native dialogs, subprocess policy)—not to grow new Qt UI code.

**Related inventory:** [Qt → Tauri + React inventory (2026-04-30)](../migration/2026-04-30-qt-to-tauri-inventory.md).

---

## Decision

1. **Single primary desktop entry:** New desktop features, UX, and product direction are implemented only on **Tauri + React** living under **`frontend/`** (web UI + `src-tauri` as the native host).

2. **No Qt sources in-repo (2026-05-01):** The former Qt/C++ tree is **deleted**; reference git tag **`pre-qt-removal-2026-05-01`** if needed.

3. **No parallel “second desktop”:** No second first-class Qt desktop is maintained in this repository.

---

## Consequences

### Positive

- One UI stack reduces cognitive load, duplicate parity work, and release ambiguity.
- BMFP boundaries stay enforceable: UI composition in `boundary`, contracts in `domain`, Tauri and persistence in `infrastructure`.
- Native concerns stay in Rust behind small, testable surfaces (`invoke` / events).

### Trade-offs and follow-through

- **CI and builds:** Pipelines and docs assume **`frontend/`** only. Release artifacts come from **Tauri** (`.github/workflows/release.yml`).

- **Onboarding:** Day-one instructions use the **Tauri** app under `frontend/` only.

- **Phase 2 ports:** Deep tool parity (hex editor, disassembler UI, Monaco terminal PTY, etc.) is scheduled as **React + Rust** work aligned with the [inventory](../migration/2026-04-30-qt-to-tauri-inventory.md) phase column—not as new Qt modules.

---

## References

- [ADR-002: Qt sources removed](./ADR-002-qt-sources-removed.md)
- [../migration/2026-04-30-qt-to-tauri-inventory.md](../migration/2026-04-30-qt-to-tauri-inventory.md) — historical path matrix (pre-removal tree).
