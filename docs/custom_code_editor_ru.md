<div align="center">

[English](custom_code_editor.md) • Русский

</div>

# Кастомный редактор кода

## Обзор

Теперь редактор кода использует собственный виджет вместо `QPlainTextEdit`.
Главная цель - работать напрямую с `FileDataBuffer`, чтобы приложение не
хранило вторую полную копию текста внутри виджета редактора.

Ключевые свойства:

- прямая работа с `FileDataBuffer`
- UTF-8-aware курсор и выделение
- индекс строк и кэш декодированных строк
- отрисовка только нужной части viewport вместо full-document виджета
- подсветка синтаксиса для распространенных исходников, конфигов, разметки и build-файлов
- синхронизация выделения с состоянием буфера и binary view

## Основные компоненты

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

## Что умеет редактор

- редактирование текста через общий буфер
- undo / redo
- copy / cut / paste
- select all
- номера строк
- word wrap
- zoom через `Ctrl + колесо мыши`
- поиск через `Ctrl+F`, `F3`, `Shift+F3`
- переход к строке через `Ctrl+G`
- контекстное меню с базовыми действиями редактора

## Поддерживаемые группы подсветки

Редактор переиспользует языковые ресурсы старого `QCodeEditor`, а там, где
их не было, использует rule-based подсветку.

Примеры:

- C / C++ / header-файлы
- assembler
- Rust
- Python
- Lua
- JSON
- Markdown
- Make / CMake
- JavaScript / TypeScript
- Java / C# / Go / PHP
- YAML / TOML / INI / ENV-подобные конфиги
- XML / XAML / SVG / Visual Studio project-файлы
- SQL
- solution-файлы (`.sln`)

## Горячие клавиши

- `Ctrl+F` - открыть встроенный поиск
- `F3` - следующее совпадение
- `Shift+F3` - предыдущее совпадение
- `Ctrl+G` - перейти к строке
- `Ctrl+C` - копировать
- `Ctrl+X` - вырезать
- `Ctrl+V` - вставить
- `Ctrl+Z` - отменить
- `Ctrl+Y` / стандартная последовательность redo - повторить
- `Ctrl+A` - выделить все
- `Tab` - вставить отступ
- `Enter` - вставить новую строку

## Заметки для разработчиков

- `CustomCodeEditor` специально сделан buffer-backed, а не document-backed.
- Логика поиска и навигации находится в `CodeEditorTab`, а логика движения
  курсора, выделения, отрисовки и редактирования - в `CustomCodeEditor`.
- В binary tab сохранен нативный поиск `QHexView`, но `Ctrl+F` теперь также
  проброшен из `BinaryTab` в RAW-страницу для удобства.
