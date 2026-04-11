# cremniy-codeeditor

[English](README.md) | [Русский](README_ru.md)

Автономный Qt-модуль редактора кода, вынесенный из `cremniy`.

## Возможности

- `CustomCodeEditor` с рендерингом поверх буфера
- `FileDataBuffer` для работы с файлами и текстом в памяти
- `LineCache`, `LineIndex` и `UTF8Decoder`
- встроенные подсветка синтаксиса и ресурсы `QCodeEditor`
- можно использовать как зависимость в `cremniy` или в любом другом Qt-проекте

## Требования

- Qt 6 Widgets
- CMake 3.16+
- C++17

## Сборка

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<path-to-Qt>"
cmake --build build
```

## Интеграция

Библиотеку можно подключить к любому Qt/CMake-проекту одним из способов:

- `git submodule`
- `git subtree`
- обычная зависимость от исходников через `add_subdirectory(...)`

Пример структуры проекта:

```text
my-project/
  external/
    cremniy-codeeditor/
  src/
  CMakeLists.txt
```

Подключение в `CMakeLists.txt`:

```cmake
add_subdirectory(external/cremniy-codeeditor)
target_link_libraries(my_app PRIVATE cremniy-codeeditor)
```

Использование публичных заголовков:

```cpp
#include "widgets/CustomCodeEditor.h"
#include "core/FileDataBuffer.h"
```

Минимальный пример:

```cpp
auto* buffer = new FileDataBuffer(this);
buffer->loadData(QByteArray("hello\nworld\n"));

auto* editor = new CustomCodeEditor(this);
editor->setBuffer(buffer);
setCentralWidget(editor);
```

## Обновление

Если проект использует этот репозиторий как зависимость:

1. обновите зависимость до нового коммита
2. пересоберите проект

Если публичный API не менялся, дополнительных правок интеграции обычно не требуется.

## Важно

Не держите в приложении локальные дубликаты этих исходников:

- `CustomCodeEditor`
- `FileDataBuffer`
- `LineCache`
- `LineIndex`
- `UTF8Decoder`

Используйте `cremniy-codeeditor` как единственный source of truth.
