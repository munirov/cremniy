<div align="center">

**Language / Язык:** open one section below (only one stays open in modern browsers).  
**English** — first block · **Русский** — второй блок.

</div>

<details name="roadmap-lang" open>
<summary><strong>English</strong></summary>

# Roadmap

## Project goal

Create a **unified tool for low-level and reverse-engineering work** — hex editor, disassembler,
code editor, connections, git, and binary analysis tools in one linked application, with an
open plugin system that can grow to cover embedded toolchains (STM32 and friends).

## Current state

The desktop application is built on **Tauri 2 + React + TypeScript**. The following is
shipped and working:

- Code editor (Monaco, syntax highlighting, search, zoom, indent settings)
- Hex / binary editor (byte-level editing, undo/redo, find/go, patch export)
- Disassembler (embedded `iced-x86` + `goblin`, instruction help, string refs, patches; optional `radare2`)
- Integrated terminal (persistent history, Cyrillic-layout correction, interrupt via Ctrl+Break)
- Reverse Calculator, Data Converter, Shellcode Generator
- **Plugin system** — contribution-based architecture (`plugins/<id>/index.tsx`)
  - Connections plugin (serial port, SSH, SFTP host manager)
  - Source Control plugin (Git status, staging, commit, advanced operations)
  - Binary Tools plugin (memory map, strings, patches, resources, symbol table, function list)
- References panel (ASCII chart, keyboard scan codes)
- Workspace search (content search across files — regex, word, glob include/exclude, replace-in-files)
- Agent / MCP control surface (`window.cremniy` commands)
- Cyrillic keyboard layout support throughout

## Near-term

- [ ] Debugger — step through execution, inspect registers and memory
- [ ] Memory visualization — visual maps of memory layout and allocation
- [ ] Hover tooltips: numbers in all bases (hex / dec / oct / bin)
- [ ] Improved terminal (PTY resize, color themes)

## Plugin system

- [ ] Auto-discovery of `plugins/` folders without manual registration in `plugins/index.ts`
- [ ] Remote plugin loading and a plugin marketplace
- [ ] STM32 / embedded toolchain packs (replacing ST's Cube/ST-LINK tooling) — long-term target

## Long-term

- Build system integration (run and build projects from within Cremniy)
- Live process inspection (memory viewer, register watch for a running process)
- Debugger with step-through, breakpoints, call stack

</details>

<details name="roadmap-lang">
<summary><strong>Русский</strong></summary>

# Дорожная карта

## Цель проекта

Создать **единый инструмент для низкоуровневой работы и обратной разработки** — HEX-редактор,
дизассемблер, редактор кода, подключения, git и инструменты анализа бинарников в одном связанном
приложении с открытой системой плагинов, которая сможет покрыть и embedded-тулчейны
(STM32 и другие).

## Текущее состояние

Десктоп-приложение на **Tauri 2 + React + TypeScript**. Работает и поставляется:

- Редактор кода (Monaco, подсветка синтаксиса, поиск, масштаб, настройки отступов)
- HEX/бинарный редактор (правка байтов, undo/redo, поиск/переход, экспорт патчей)
- Дизассемблер (встроенный `iced-x86` + `goblin`, справка по инструкциям, строковые ссылки, патчи; опционально `radare2`)
- Встроенный терминал (история команд, коррекция кириллической раскладки, прерывание через Ctrl+Break)
- Обратный калькулятор, конвертер данных, генератор шелл-кода
- **Система плагинов** — вклады (`plugins/<id>/index.tsx`)
  - Плагин Connections (serial-порт, SSH, SFTP)
  - Плагин Source Control (Git-статус, стейджинг, коммит, расширенные операции)
  - Плагин Binary Tools (карта памяти, строки, патчи, ресурсы, таблица символов, функции)
- Панель References (таблица ASCII, скан-коды клавиш)
- Поиск по рабочему пространству (контентный поиск по файлам — regex, слово, glob include/exclude, replace-in-files)
- Командная поверхность для агентов/MCP (`window.cremniy`)
- Поддержка кириллической раскладки клавиатуры

## Ближайшие задачи

- [ ] Отладчик — пошаговое выполнение, просмотр регистров и памяти
- [ ] Визуализация памяти — наглядные карты расположения памяти
- [ ] Всплывающие подсказки: числа во всех системах счисления (hex / dec / oct / bin)
- [ ] Улучшенный терминал (ресайз PTY, цветовые темы)

## Система плагинов

- [ ] Авто-дискавери папок `plugins/` без ручной регистрации в `plugins/index.ts`
- [ ] Удалённая загрузка плагинов и маркетплейс
- [ ] Паки для STM32 / embedded-тулчейна (замена Cube/ST-LINK от ST) — долгосрочная цель

## Долгосрочные

- Интеграция системы сборки (запуск и сборка проектов прямо из Cremniy)
- Инспекция живых процессов (просмотр памяти, регистры запущенного процесса)
- Отладчик с пошаговым выполнением, точками останова, стеком вызовов

</details>
