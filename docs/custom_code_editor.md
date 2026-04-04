<div align="center">

English • [Русский](custom_code_editor_ru.md)

</div>

# Custom Code Editor

## Overview

The code editor now uses a custom widget instead of `QPlainTextEdit`.
Its main goal is to work directly with `FileDataBuffer` so the application
does not keep a second full text copy inside the editor widget.

Key properties:

- direct work with `FileDataBuffer`
- UTF-8 aware cursoring and selection
- line index and decoded line cache
- viewport-based rendering instead of full-document widgets
- syntax highlighting for common source, config, markup, and build files
- synchronized selection with the binary view buffer state

## Main Components

- `src/widgets/CustomCodeEditor.h`
- `src/widgets/CustomCodeEditor.cpp`
- `src/widgets/LineNumberArea.h`
- `src/widgets/LineNumberArea.cpp`
- `src/core/FileDataBuffer.h`
- `src/core/FileDataBuffer.cpp`
- `src/core/LineIndex.h`
- `src/core/LineIndex.cpp`
- `src/core/LineCache.h`
- `src/core/LineCache.cpp`
- `src/core/UTF8Decoder.h`
- `src/core/UTF8Decoder.cpp`

## Supported Editor Features

- text editing through the shared buffer
- undo / redo
- copy / cut / paste
- select all
- line numbers
- word wrap
- zoom with `Ctrl + Mouse Wheel`
- search with `Ctrl+F`, `F3`, `Shift+F3`
- go to line with `Ctrl+G`
- context menu with common editing actions

## Supported Highlighting Groups

The editor reuses the old `QCodeEditor` language assets where possible and
adds rule-based highlighting for common formats.

Examples:

- C / C++ / headers
- assembly
- Rust
- Python
- Lua
- JSON
- Markdown
- Make / CMake
- JavaScript / TypeScript
- Java / C# / Go / PHP
- YAML / TOML / INI / ENV-like configs
- XML / XAML / SVG / Visual Studio project files
- SQL
- solution files (`.sln`)

## User Shortcuts

- `Ctrl+F` - open inline search
- `F3` - next match
- `Shift+F3` - previous match
- `Ctrl+G` - go to line
- `Ctrl+C` - copy
- `Ctrl+X` - cut
- `Ctrl+V` - paste
- `Ctrl+Z` - undo
- `Ctrl+Y` / standard redo sequence - redo
- `Ctrl+A` - select all
- `Tab` - insert indentation
- `Enter` - insert new line

## Notes For Developers

- `CustomCodeEditor` is intentionally buffer-backed, not document-backed.
- Search and navigation logic live in `CodeEditorTab`, while text movement,
  selection, rendering, and editing live in `CustomCodeEditor`.
- The binary tab keeps its own native search dialog through `QHexView`, but
  `Ctrl+F` is now wired from `BinaryTab` to the RAW page for convenience.
