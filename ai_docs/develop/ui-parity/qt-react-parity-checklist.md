# Qt → React parity checklist (canonical)

**BMFP:** `cremniy/documentation/EN/bmfp_and_layers.md`.  
**Qt snapshot:** tag `pre-qt-removal-2026-05-01`; reference tree `cremniy-main/src/` (see `cremniy/documentation/REFERENCE_QT.md`).  
**Related:** workspace-root `ai_docs/develop/ui-parity/2026-04-29-react-parity-checklist.md` (dated smoke checklist); this file adds **Qt path** and **BMFP layer** columns for porting.

**Status legend:** `done` (acceptable shell parity), `partial`, `missing`, `phase2` (heavy parity still outstanding inside the shell).

| Qt path (under `src/`) | Behavior (short) | React module / area | BMFP layer | Status |
|-------------------------|------------------|---------------------|------------|--------|
| `main.cpp` | App bootstrap, load QSS, show welcome | `source/frontend/src/main.tsx`, `App.tsx` | boundary | partial |
| `app/WelcomeWindow/welcomeform.*` | Recent list, Open, Create project | `boundary/welcome/WelcomeView.tsx` | boundary | done |
| `utils/projectshistorymanager.*` | Recent workspaces persistence | `domain/preferences/appPreferences.ts`; `infrastructure/preferences/preferencesBridge.ts` | domain; infrastructure | done |
| `app/IDEWindow/idewindow.*` | IDE splits, menu region, central layout; project path semantics | `boundary/RootApp.tsx`; `boundary/layout/RootLayout.tsx`; `boundary/layout/IdeWorkspace.tsx`; `boundary/workspace/WorkspaceContext.tsx`; `domain/workspace/types.ts` | boundary; domain | partial |
| `widgets/filetreeview.*` | Project file tree | `boundary/workspace/WorkspaceFileTree.tsx` in `IdeWorkspace` | boundary | partial |
| `ui/filestabwidget.*`, `widgets/filetab.*` | Tabs; keyboard cycle; middle-click close; dirty close guard | `boundary/layout/IdeEditorTabStrip.tsx`; session in `boundary/workspace/IdeSessionContext.tsx` | boundary (+ session) | partial |
| `ui/toolstabwidget.*`, `widgets/verticaltab*` | Tool tab stack | `boundary/layout/IdeToolDock.tsx`; `boundary/workspace/ToolDockContext.tsx`; `domain/toolTabs/*` | boundary; domain | partial |
| `widgets/terminal/terminalwidget.*` | Docked terminal | `RootLayout` footer; `viewMenu` / prefs `terminalPanelVisible` | boundary | partial |
| `ui/MenuBar/**`, `Menus/**` | Main menus + actions | `boundary/chrome/MenuBar.tsx`; shortcuts `domain/menu/menuShortcuts.ts`; `RootApp.tsx` wiring | boundary (+ domain) | partial |
| `dialogs/settingsdialog.*` | Settings UI | `boundary/settings/SettingsDialog.tsx` + prefs domain/infra | boundary; domain; infrastructure | partial |
| `dialogs/filecreatedialog.*` | New file from tree | Tree context + workspace-scoped Tauri commands | boundary; infrastructure | partial |
| `dialogs/reversecalculatordialog.*` | Tools calculator | `boundary/tools/ReverseCalculatorDialog.tsx`; `domain/reverseCalculator/*` | boundary; domain | partial |
| `resources/styles/style.qss`, `resources/cremniy_res.qrc` | Global theme, icons | `shared/theme/tokens.css`; `public/icon.png` | shared | partial |
| `ToolTabs/Binary/**` | Hex / binary tools | `boundary/tools/BinaryToolPanel.tsx` (stub host) | boundary (+ domain) | phase2 |
| `ToolTabs/CodeEditor/**` | Code editor tool | `boundary/tools/CodeEditorToolPanel.tsx` (stub host) | boundary (+ domain) | phase2 |
| `ToolTabs/Disassembler/**` | Disassembler | `boundary/tools/DisassemblerToolPanel.tsx` (stub host) | boundary (+ domain) | phase2 |
| `core/ToolTabFactory.*`, `FileDataBuffer.*` | Tab/buffer contracts | `domain/workspace/fileBufferTypes.ts` (minimal types); session buffers in `IdeSessionContext` | domain | partial |

---

## Ownership

Orchestration slices and wave ownership are tracked in workspace-root `ai_docs/develop/migration/2026-04-30-qt-to-tauri-inventory.md`. Update this table when a row moves from `missing` → `partial` → `done`.
