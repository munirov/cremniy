# Qt UI audit — visual parity reference (2026-04-29)

**Historical:** Paths in this file referred to **`src/`**, removed 2026-05-01. Use git tag **`pre-qt-removal-2026-05-01`** to inspect the old Qt sources.

Static reference from the former Qt6 desktop sources. Use this with BMFP/Tauri parity work; **no screenshots are stored in-repo**.

---

## Entry points

| Role | Path |
|------|------|
| Application bootstrap | `cremniy/src/main.cpp` |
| Welcome launcher | `cremniy/src/app/WelcomeWindow/welcomeform.cpp`, `welcomeform.h` |
| IDE shell | `cremniy/src/app/IDEWindow/idewindow.cpp`, `idewindow.h` |

### Flow (`main.cpp`)

- Builds `QApplication`, sets org/app names (`cremniy` / `Cremniy`).
- Window icon: `:/icons/icon.png` from resources.
- Loads global stylesheet from `:/styles/style.qss` (`QFile` + `setStyleSheet` on the application).
- Instantiates **`WelcomeForm`**, shows it, runs the event loop.

### Welcome → IDE

- **`WelcomeForm::OpenProject`**: persists history via `utils::ProjectsHistoryManager`, hides welcome, constructs **`IDEWindow(path)`**, connects `IDEWindow::CloseProject` to show welcome again, shows IDE maximized.
- **`WelcomeForm::L2CreateButton`**: validates create-project fields; on success constructs **`IDEWindow(new_project_path)`** and destroys the welcome widget.

---

## Widget / layout hierarchy

### Welcome (`WelcomeForm`)

Root: **`QWidget`** (title `Cremniy`, default **400×300**).

- **`QVBoxLayout`** (root)
  - **`QStackedWidget`** (`stack`)
    - **Page 0 — Welcome**
      - **`QVBoxLayout`**
        - **`QListView`** — recent projects (`RecentProjectsList`; single selection, no edit triggers).
        - **`QHBoxLayout`** — **`QPushButton`** "Open" | "Open..." | "Create".
    - **Page 1 — Create project**
      - **`QVBoxLayout`**
        - **`QGridLayout`** — labels + **`QLineEdit`** (project name), **`QComboBox`** languages (`C`, `C++`, `ASM`, `C + ASM`, `Custom`), **`ClickableLineEdit`** (path, read-only).
        - Stretch.
        - **`QLabel`** `info_label` — validation/errors (inline styles use **`#bf3131`** when visible).
        - **`QHBoxLayout`** — "Create" | "Back".

**Note:** Recent-project "Open" enables **`state="green"`** on selection (`SelectProjectInList`) for QSS-driven styling.

### IDE (`IDEWindow`)

Root: **`QMainWindow`** (title `Cremniy`, starts maximized).

- **`QMenuBar`** — populated by **`MenuBarBuilder`** (`menuBar()->setNativeMenuBar(false)` — in-window menu strip).
- **`QStatusBar`** — referenced (`statusBar()`).
- **Central:** **`QWidget`** + **`QHBoxLayout`** (`m_mainLayout`, zero margins).
  - **`QSplitter`** **`m_verticalSplitter`** (**Vertical**)
    - Child 0: **`QSplitter`** **`m_mainSplitter`** (**Horizontal**) — upper IDE band.
      - Left: **`QWidget`** + **`QVBoxLayout`** (`leftLayout`, zero margins).
        - **`FileTreeView`** `m_filesTreeView` (`cremniy/src/widgets/filetreeview.*`).
        - **Implementation detail:** `idewindow.cpp` adds `m_filesTreeView` to `leftLayout` twice (lines ~47 and ~59); parity should mirror **one** tree in the sidebar column unless deliberately reproducing this quirk.
      - Right: **`FilesTabWidget`** `m_filesTabWidget`, **`objectName`** `"filesTabWidget"` (`cremniy/src/ui/filestabwidget.*`) — editor tabs (closable, movable).
    - Initial vertical sizes **`{800, 200}`** — reserved ratio for bottom strip; **`TerminalWidget`** is **lazy**: created only when **View → Show terminal** is toggled on (`on_Toggle_Terminal`), then appended to **`m_verticalSplitter`**.
- **`m_mainSplitter`** sizes **`{200, 1000}`** (~sidebar vs editor); both panes non-collapsible; tree **`minimumWidth` 180**, columns 1–3 hidden, header hidden.

### Terminal region

- **`TerminalWidget`** — `cremniy/src/widgets/terminal/terminalwidget.*`.
- Not shown until **`ViewMenu`** terminal action is checked; then splitter adjusts again to **`{800, 200}`**.

---

## Menu bar (`MenuBarBuilder` + `MenuFactory`)

| Path | Purpose |
|------|---------|
| `cremniy/src/ui/MenuBar/menubarbuilder.cpp` | Adds each registered menu to `QMenuBar`. |
| `cremniy/src/ui/MenuBar/menufactory.cpp` | Singleton registry; `availableMenus()` returns keys **`"1"`…`"6"`** (iteration order follows container rules — verify at runtime if exact left-to-right order matters). |

Menus register statically in each menu’s `.cpp` (numeric IDs):

| ID | Class | Title | Rough contents |
|----|-------|-------|----------------|
| `"1"` | `FileMenu` | **File** | **Note:** QAction labels are swapped vs variable names — UI shows **"New Project"** then **"Open Project"**, separator, **"Save File"** (shortcut Save), separator, **"Close Project"**. |
| `"2"` | `EditMenu` | **Edit** | Separator, **Settings** (`Ctrl+,` / `Ctrl+б`). |
| `"3"` | `ViewMenu` | **View** | **Word Wrap** (checkable, default on), separator, **Show terminal** (checkable; `Ctrl+\`` / `Ctrl+ё`). |
| `"4"` | `BuildMenu` | **Build** | Empty menu shell (no actions in constructor). |
| `"5"` | `ToolsMenu` | **Tools** | **Reverse Calculator** (`Ctrl+Shift+R`). |
| `"6"` | `ReferencesMenu` | **References** | **ASCII characters**, **Keyboard Scancodes**. |

Paths:

- `cremniy/src/ui/MenuBar/Menus/File/filemenu.cpp`
- `cremniy/src/ui/MenuBar/Menus/Edit/editmenu.cpp`
- `cremniy/src/ui/MenuBar/Menus/View/viewmenu.cpp`
- `cremniy/src/ui/MenuBar/Menus/Build/buildmenu.cpp`
- `cremniy/src/ui/MenuBar/Menus/Tools/toolsmenu.cpp`
- `cremniy/src/ui/MenuBar/Menus/References/referencesmenu.cpp`

---

## Styles and resources

### Global QSS

| Path | Role |
|------|------|
| `cremniy/src/resources/styles/style.qss` | Loaded at startup via `:/styles/style.qss`. |

**Representative tokens / selectors:**

- Base: **`QWidget`** — bg `#262626`, text `#ffffff`, font **JetBrains Mono / Consolas**, **12px**.
- Inputs/lists: **`QLineEdit`**, **`QPlainTextEdit`**, **`QListView`**, **`QTreeView`** — dark panels **`#1f1f1f`**, borders `#262626` / `#1f1f1f`; selection **`#333333`**.
- Buttons: hover **`#162033`**, border accent **`#2c4c7c`**; **`QPushButton[state="green"]`** border **`#2c7c32`** (welcome “Open” when recent row selected).
- **`#filesTabWidget`** tabs — close icon `:/icons/close.png`; selected tab gradient accent **`#2626d5`**; min tab width **200px**.
- **`#toolTabWidget`** — alternate tab strip styling (smaller tabs, hex/tool contexts).
- **`QListWidget#hexTabsList`** — hex sidebar list styling.
- **`QMenuBar` / `QMenu`** — bar hover `#444444` / `#262626`; menu border `#111111`; menu selected `#333333`; checkbox indicators green **`#2c7c32`** when checked.
- **Scrollbars** — track `#1f1f1f`, handle `#262626`, **16px**.
- **`QWidget#searchWidget`** — nested search UI (focus border `#2626d5`, error state reds).

### Qt resource bundle

| Path | Purpose |
|------|---------|
| `cremniy/src/resources/cremniy_res.qrc` | Prefix `/`: `styles/style.qss`, `icons/*.png` (`icon`, `binary`, `dasm`, `code`, `close`), `data/instructions_ru.json`. |

Secondary bundle (editor submodule): `cremniy/src/ToolTabs/CodeEditor/QCodeEditor/resources/codeeditor_res.qrc`.

---

## Screenshot checklist (manual — for 1:1 parity later)

Capture **running Qt app** (no binaries in repo). Suggested set:

**Welcome**

1. Welcome page — empty recent list, three buttons default state.
2. Welcome page — recent list populated; row selected — **Open** shows green/outline styling (`state="green"`).
3. Create Project page — default layout (grid + Create/Back).
4. Create Project — validation: empty project name (red labels / info label).
5. Create Project — invalid or duplicate path / directory errors (`info_label` visible).

**IDE shell**

6. IDE maximized — horizontal splitter (~200 vs editor); **FilesTabWidget** visible with **no tabs** (initial strip empty).
7. IDE — **FileTreeView** expanded with files; **tabs open** with one tab selected (gradient underline **`#2626d5`**).
8. IDE — **View → Show terminal** off vs on (bottom pane appears; splitter behavior).

**Menus**

9. Menu bar — each top-level menu dropped: File, Edit, View, Build, Tools, References (structure only).

**Tree**

10. Context menu — empty area (project root): Create File / Create Folder.
11. Context menu — folder node: Open, Rename, Delete, Create File/Folder.
12. Context menu — file node: Open, Rename, Delete.

**Dialogs (secondary parity)**

13. **Settings** (`Edit → Settings`) — window chrome + layout.
14. **Reverse Calculator** (`Tools`) — centered modal-ish dialog.
15. Optional: **File create** dialog from tree context menu.

---

## Key file paths (repo-relative)

```
cremniy/src/main.cpp
cremniy/src/app/WelcomeWindow/welcomeform.cpp
cremniy/src/app/WelcomeWindow/welcomeform.h
cremniy/src/app/IDEWindow/idewindow.cpp
cremniy/src/app/IDEWindow/idewindow.h
cremniy/src/ui/filestabwidget.cpp
cremniy/src/ui/filestabwidget.h
cremniy/src/widgets/filetreeview.cpp
cremniy/src/widgets/filetreeview.h
cremniy/src/widgets/terminal/terminalwidget.cpp
cremniy/src/widgets/terminal/terminalwidget.h
cremniy/src/widgets/clickablelineedit.h
cremniy/src/utils/projectshistorymanager.cpp
cremniy/src/utils/projectshistorymanager.h
cremniy/src/ui/MenuBar/menubarbuilder.cpp
cremniy/src/ui/MenuBar/menubarbuilder.h
cremniy/src/ui/MenuBar/menufactory.cpp
cremniy/src/ui/MenuBar/menufactory.h
cremniy/src/resources/styles/style.qss
cremniy/src/resources/cremniy_res.qrc
```
