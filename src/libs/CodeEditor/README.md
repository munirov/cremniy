# cremniy-codeeditor

[English](README.md) | [Русский](README_ru.md)

Standalone Qt code editor module extracted from `cremniy`.

## Features

- `CustomCodeEditor` with buffer-backed rendering
- `FileDataBuffer` for file and in-memory text storage
- `LineCache`, `LineIndex`, and `UTF8Decoder`
- bundled `QCodeEditor` syntax highlighting and resources
- reusable as a dependency in `cremniy` or any other Qt project

## Requirements

- Qt 6 Widgets
- CMake 3.16+
- C++17

## Build

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<path-to-Qt>"
cmake --build build
```

## Integration

You can connect the library to any Qt/CMake project in one of these ways:

- `git submodule`
- `git subtree`
- regular source dependency with `add_subdirectory(...)`

Example project structure:

```text
my-project/
  external/
    cremniy-codeeditor/
  src/
  CMakeLists.txt
```

Add the library in `CMakeLists.txt`:

```cmake
add_subdirectory(external/cremniy-codeeditor)
target_link_libraries(my_app PRIVATE cremniy-codeeditor)
```

Use the public headers in your code:

```cpp
#include "widgets/CustomCodeEditor.h"
#include "core/FileDataBuffer.h"
```

Minimal usage example:

```cpp
auto* buffer = new FileDataBuffer(this);
buffer->loadData(QByteArray("hello\nworld\n"));

auto* editor = new CustomCodeEditor(this);
editor->setBuffer(buffer);
setCentralWidget(editor);
```

## Updating

If your project uses this repository as a dependency:

1. update the dependency to a newer commit
2. rebuild your project

As long as the public API stays the same, no extra integration changes are needed.

## Important

Do not keep duplicate local copies of these sources in your app:

- `CustomCodeEditor`
- `FileDataBuffer`
- `LineCache`
- `LineIndex`
- `UTF8Decoder`

Use `cremniy-codeeditor` as the single source of truth.
