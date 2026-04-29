# ADR-002: Qt/C++ sources removed from the repository

**Status:** Accepted  
**Date:** 2026-05-01  
**Supersedes / amends:** [ADR-001](./ADR-001-tauri-desktop-primary.md) (Qt was “deprecated”; this ADR records “absent from tree”)

## Context

The legacy desktop IDE was implemented with **Qt 6 and C++** under `src/`. That stack covered far more than visuals: windowing, widgets, threading, file and project integration, tool tabs, disassembler worker glue, terminal embedding, and resources.

The product direction is a **single** in-repo desktop implementation: **Tauri + React** under `frontend/`, with **Rust** for the native shell and controlled I/O.

## Decision

1. The **`src/`** tree (all Qt/C++ application sources and nested CMake) has been **deleted** from the repository.
2. No `CMakeLists.txt` remains for the former Qt application.
3. CI **never** installs Qt for this product; **release** workflows produce **Tauri** bundles only.
4. The last commit that still contained `src/` is pointed to by git tag **`pre-qt-removal-2026-05-01`** for historical reference.

## Consequences

- **Pros:** simpler onboarding, no dual stack, no accidental Qt CI cost, one clear codebase for new features.
- **Cons:** parity work must use the tag (or git history), not a live `src/` checkout; some `ai_docs` tables still describe the old paths as **historical** migration targets.

## References

- [Qt zero inventory](../migration/2026-05-01-qt-zero-inventory.md)
- [ADR-001](./ADR-001-tauri-desktop-primary.md)
