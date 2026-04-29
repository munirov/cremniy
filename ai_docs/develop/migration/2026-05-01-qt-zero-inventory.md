# Qt removal inventory (2026-05-01)

## Purpose

Final inventory before deleting all Qt/C++ IDE sources from the repository. Qt is **not** “only UI”: it provided the full legacy desktop stack (widgets, layouts, threading, file tooling, menus, tool tabs, disassembler integration, terminal widget, etc.).

## Components removed

| Area | Location (before deletion) |
|------|----------------------------|
| Main IDE CMake | `src/CMakeLists.txt` |
| Application entry, Welcome, IDE window | `src/main.cpp`, `src/app/**` |
| Widgets, UI, menus | `src/widgets/**`, `src/ui/**` |
| Dialogs | `src/dialogs/**` |
| Core (tabs, buffers) | `src/core/**` |
| Utils | `src/utils/**` |
| Resources (QRC, QSS, icons, JSON) | `src/resources/**` |
| Tool tabs (Binary, Code editor, Disassembler) | `src/ToolTabs/**` |
| Nested CMake | `src/**/CMakeLists.txt` |

## Repository touchpoints updated

- `.github/workflows/qt-legacy-ci.yml` — **deleted**
- `.github/workflows/release.yml` — **Tauri-only**
- `README.md`, `README_ru.md`, `CONTRIBUTING*.md`, `ROADMAP*.md` — Qt/CMake paths removed
- `cremniy.desktop` — repointed or removed (see repo)
- `ai_docs/**` — migration / ADR addendum

## Pre-removal reference

Create a git tag on the last commit that still contains `src/` (e.g. `pre-qt-removal-2026-05-01`) before deleting, so the old tree remains reachable in history or tag.
