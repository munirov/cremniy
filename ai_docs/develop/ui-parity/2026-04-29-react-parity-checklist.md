# React shell vs Qt — parity checklist (2026-04-29)

Smoke checklist for BMFP-boundary UI scaffolding vs Qt references (historical `src/` paths — tree removed 2026-05-01; see tag **`pre-qt-removal-2026-05-01`**). **`Status`** is target completeness for **visual/UI shell** only — no analyzer features.

**Migration ownership matrix:** `ai_docs/develop/migration/2026-04-30-qt-to-tauri-inventory.md` (Qt areas → BMFP layers / Tauri / wave vs phase2).

| Qt screen / element | React route / component(s) | Status | Notes |
|---------------------|-----------------------------|--------|-------|
| Welcome window (`WelcomeForm`, recent list + Open / Create) | Route `/`; `boundary/welcome/WelcomeView.tsx` | Parity | Mock data; layout aligns with launcher; shortcuts/history wired later. |
| IDE window shell (`IDEWindow`; main split regions) | Route `/ide`; `boundary/RootApp.tsx` composing `layout/RootLayout.tsx` + `layout/IdeWorkspace.tsx` | Parity | Static shell; draggable split ratios not mirrored yet. |
| In-window menu strip (`MenuBarBuilder` / menus 1–6) | `boundary/chrome/MenuBar.tsx` (used in `RootApp`) | Stub | Labels/structure stubs; actions non-functional per scope. |
| Left files tree sidebar (`FileTreeView` column) | `IdeWorkspace` → `aside[aria-label="Files"]` | Stub | Placeholder strip; tree UI pending. |
| Editor tab strip (`FilesTabWidget` / `#filesTabWidget`) | `IdeWorkspace` → region “Open tabs placeholder” (`tabStrip`) | Stub | No closable/movable tabs yet. |
| Editor body / central editor | `IdeWorkspace` → `section[aria-label="Editor"]`; default child from `RootApp` | Stub | Holds title placeholder; real editors out of shell scope. |
| Bottom terminal / split (`TerminalWidget`, View → Show terminal) | `RootLayout` → `footer[aria-label="Terminal"]`; `shellFooterInner` placeholder | Stub | Qt hides terminal until menu toggle — React always shows a footer strip placeholder for layout smoke. |
| Global QSS palette & typography tokens | `shared/theme/tokens.css` (+ `index.css` imports) | Parity | Ported palette/spacing/fonts; selectors differ (CSS Modules vs object names). |
| Window / app icons (`:/icons/icon.png`) | Web: `frontend/public/icon.png` + `<link rel="icon">` in `frontend/index.html`; bundle: `frontend/src-tauri/icons/*` | Parity | Original source was under removed `src/resources/icons/`; keep in sync with `frontend/public/icon.png`. |
