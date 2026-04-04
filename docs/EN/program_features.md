# Program Features (v0.1.3)

English • [Русский](../RU/program_features.md)

This document enumerates **all currently implemented features** in Cremniy and where to find them in the UI.

**Related docs:** [Overview](overview.md) · [User Guide](user_guide.md) · [Hotkeys](hotkey.md)

## Table of contents

- [1) Application windows and layout](#1-application-windows-and-layout)
  - [1.1 Welcome window](#11-welcome-window)
  - [1.2 Create Project page](#12-create-project-page)
  - [1.3 IDE main window](#13-ide-main-window)
  - [1.4 Panel layout & splitters](#14-panel-layout--splitters)
- [2) Project / file tree](#2-project--file-tree)
  - [2.1 Tree behavior](#21-tree-behavior)
  - [2.2 Context menu (right click)](#22-context-menu-right-click)
- [3) File tabs (top tab bar)](#3-file-tabs-top-tab-bar)
  - [3.1 Opening and managing tabs](#31-opening-and-managing-tabs)
  - [3.2 Pinning tabs](#32-pinning-tabs)
  - [3.3 Closing tabs](#33-closing-tabs)
  - [3.4 Tab navigation shortcuts](#34-tab-navigation-shortcuts)
- [4) Tool tabs (per file)](#4-tool-tabs-per-file)
  - [4.1 Code Editor tab](#41-code-editor-tab)
  - [4.2 Binary tab (HEX / formats)](#42-binary-tab-hex--formats)
  - [4.3 Disassembler tab](#43-disassembler-tab)
- [5) Reverse Calculator (Tools → Reverse Calculator)](#5-reverse-calculator-tools--reverse-calculator)
- [6) Settings (Edit → Settings)](#6-settings-edit--settings)
  - [6.1 Disassembler backend](#61-disassembler-backend)
  - [6.2 Disassembler options](#62-disassembler-options)
  - [6.3 Import / Export](#63-import--export)
- [7) References](#7-references)
  - [7.1 ASCII Characters (References → ASCII Characters)](#71-ascii-characters-references--ascii-characters)
- [8) Embedded terminal](#8-embedded-terminal)
- [9) Menus and actions (full list)](#9-menus-and-actions-full-list)
  - [9.1 File](#91-file)
  - [9.2 Edit](#92-edit)
  - [9.3 View](#93-view)
  - [9.4 Build](#94-build)
  - [9.5 Tools](#95-tools)
  - [9.6 References](#96-references)
- [10) Keyboard shortcuts](#10-keyboard-shortcuts)

---

## 1) Application windows and layout

### 1.1 Welcome window
- **Recent projects list**: list of previously opened project folders.
- **Open**: opens the selected recent project.
- **Open…**: browse and open an existing folder.
- **Create**: switches to the project creation page.

### 1.2 Create Project page
- **Project Name** (validated: letters, numbers, `_` and `-`).
- **Language** selector: C / C++ / ASM / C + ASM / Custom.
- **Path** selector: choose parent directory for the new project.
- **Create**: validates and creates a project folder, then opens IDE.
- **Back**: return to the Welcome page.

### 1.3 IDE main window
- **Menu bar** (File / Edit / View / Build / Tools / References).
- **Left sidebar**: file tree (project browser).
- **Main workspace**: file tabs and tool tabs.
- **Status bar**: standard Qt status bar.
- **Terminal panel**: embedded terminal (toggle via View).

### 1.4 Panel layout & splitters
- Horizontal splitter: left sidebar vs. main workspace.
- Vertical splitter: workspace vs. terminal panel.
- Panels are non‑collapsible for the main workspace; terminal is collapsible.

---

## 2) Project / file tree

### 2.1 Tree behavior
- Uses OS filesystem (read/write).
- Single‑click selects nodes; double‑click opens files.
- Columns except the name are hidden; header hidden.
- Visibility can be toggled via **View → File Tree**.

### 2.2 Context menu (right click)
**On a folder:**
- **Open** (expand directory).
- **Rename** (inline edit).
- **Delete** (with confirmation dialog).
- **Create File** (opens “Create file” dialog).
- **Create Folder** (opens “Create folder” dialog).

**On a file:**
- **Open** (open in tabs).
- **Rename** (inline edit).
- **Delete** (with confirmation dialog).

**On empty space:**
- **Create File** (in project root).
- **Create Folder** (in project root).

---

## 3) File tabs (top tab bar)

### 3.1 Opening and managing tabs
- Each opened file becomes a tab.
- Re‑opening an already opened file focuses its tab.
- **Modified indicator**: `*` is appended to tab title when data differs.

### 3.2 Pinning tabs
- **Pin/Unpin** via tab context menu.
- **Pinned tabs** are moved to the beginning of the tab bar.
- Pinned tabs show a **📌** icon (theme icon `emblem-pinned` if available).
- **Pinned tabs cannot be closed** until unpinned.
- Drag‑move rules:
  - pinned tabs can be reordered **only within the pinned region**;
  - unpinned tabs cannot be moved into the pinned region.

### 3.3 Closing tabs
- Click close button, **Ctrl+W**, or **middle‑click**.
- If modified, prompts to **Save / Don’t Save / Cancel**.

### 3.4 Tab navigation shortcuts
- **Alt + Left/Right**: switch between file tabs.
- **Alt + Mouse Wheel**: switch between file tabs.

---

## 4) Tool tabs (per file)

Each file tab contains tool tabs sharing one synchronized data buffer.
When one tool modifies data, the “*” indicator appears on the file tab; saving clears it and refreshes other tools.

### 4.1 Code Editor tab
- **Syntax highlighting** and **autocompletion** for:
  - C / C++ (`.c`, `.h`, `.cpp`, `.hpp`)
  - ASM (`.asm`, `.s`)
  - Rust (`.rs`)
  - Make (`Makefile`, `.mk`, `.make`)
  - CMake (`CMakeLists.txt`, `.cmake`)
- **Line numbers** and current line highlight.
- **Bracket matching** (parentheses highlight).
- **Auto‑indent** and auto‑parentheses insertion.
- **Tab replaces with 4 spaces** (with Backtab unindent).
- **Zoom**: **Ctrl + Plus / Ctrl + Minus**.
- **Search bar**:
  - **Ctrl+F** to open, **Esc** to close.
  - Find Next / Find Prev buttons.
  - Live search with wrap‑around.
- **Binary file detection** overlay:
  - Shows “Binary file detected” with **Open anyway**.
- **Selection sync**: selection changes are propagated to other tools.

### 4.2 Binary tab (HEX / formats)
The Binary tool contains format pages, all synchronized with the shared buffer:

- **RAW**: byte‑level HEX editor (QHexView).
  - Edit bytes directly (in‑place patching).
  - Selection sync with other tools.
  - Context menu: **Select All / Copy / Paste**.
  - Standard shortcuts: Undo / Redo / Cut / Copy / Paste / Select All.
  - Find/Replace dialog (Text/Hex/Int/Float, forward/backward/all).

- **ELF**: header inspection.
  - Validates ELF signature and size.
  - Displays Class, Endianness, Type, Machine.

- **PE**: header inspection.
  - Validates DOS signature + PE signature.
  - Displays Machine, Sections count, Characteristics.

- **MBR**: Master Boot Record inspection.
  - Validates 0x55AA signature.
  - Displays partition entries: active flag, type, LBA, size, description.

### 4.3 Disassembler tab
- **Backend selection**: uses **objdump** or **radare2** (see Settings).
- **Toolbar**:
  - **Disassemble** / **Cancel**.
  - **Section** selector (filters output).
  - **Search** by address / mnemonic / operands.
  - **Log** toggle (diagnostic panel).
- **Functions panel**:
  - List of detected functions with filter and jump.
- **Listing view**:
  - Fixed‑column, IDA‑like layout.
  - Auto comments for referenced strings.
  - Tooltip help for instructions (mnemonic / flags / numeric conversion).
- **Context menu (listing)**:
  - Copy address, Copy bytes.
  - Hex patch at instruction (radare2 backend only).
  - Patch bytes (same length).
  - Patch string bytes (radare2 backend only).
  - Show string bytes (raw bytes + ASCII preview).
- **F5**: re‑analyze / disassemble again.

---

## 5) Reverse Calculator (Tools → Reverse Calculator)

- Input supports **decimal**, **hex (0x)**, **binary (0b)** and **signed** values.
- Bit width: 8 / 16 / 32 / 64.
- **Show signed** toggle.
- **Swap endian**.
- Copy buttons: **Copy hex / dec / bin**.

---

## 6) Settings (Edit → Settings)

### 6.1 Disassembler backend
- Choose **objdump** or **radare2**.
- Configure paths (auto‑resolved from PATH when empty).
- Status indicators show **found / missing** for:
  - objdump
  - radare2
  - `file` (dependency used by objdump for arch detection)

### 6.2 Disassembler options
- Instruction limit per section.
- Assembly syntax: **Intel / AT&T**.
- Radare2 analysis: **None / aa / aaa**.
- Optional radare2 pre‑commands (one per line).

### 6.3 Import / Export
- **Test**: check availability of backend tools.
- **Import…** / **Export…** settings in INI format.

---

## 7) References

### 7.1 ASCII Characters (References → ASCII Characters)
- Table of 0–255 ASCII codes.
- Columns: **Char / Decimal / Hex**.
- Search by **character**, **decimal**, or **hex**.
- Auto‑selects and scrolls to matched row.

---

## 8) Embedded terminal

- Toggle with **View → Show terminal**.
- Runs **PowerShell** on Windows, **bash** on Unix.
- Maintains command history (stored in app data).
- Handles Ctrl+C for process interrupt.

---

## 9) Menus and actions (full list)

### 9.1 File
- **New Project** (not implemented yet).
- **Open Project** (not implemented yet).
- **Save File** (**Ctrl+S**).
- **Close Project** (closes IDE window, returns to Welcome).

### 9.2 Edit
- **Settings** (**Ctrl+,** / **Ctrl+б**).

### 9.3 View
- **Word Wrap** (toggle).
- **Show terminal** (**Ctrl+` / Ctrl+ё**).
- **File Tree** (**Ctrl+B**).

### 9.4 Build
- Placeholder menu (no actions yet).

### 9.5 Tools
- **Reverse Calculator** (**Ctrl+Shift+R**).

### 9.6 References
- **ASCII Characters**.

---

## 10) Keyboard shortcuts

Full list: **[Hotkeys](hotkey.md)**.
