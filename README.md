<div align="center">
  <img src="src/resources/icons/icon.png" width="250" alt="logo">
  <br>
  <h3>Cremniy</h3>
  <h6>A development environment for low-level programming that combines all low-level tools into a single application</h6>

[![License](https://img.shields.io/github/license/igmunv/cremniy?color=orange&style=flat-square)](LICENSE)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)

English • [Русский](README_ru.md)

</div>

<br>

## 📌 About the Project

**Cremniy** — a development environment for low-level programming.

It combines tools for working with binary files, memory, and system code in a single application.

### The project is focused on

- system software developers
- reverse engineers
- information security specialists
- embedded systems developers

## ✨ Features

### Current

- Code editor
- HEX editor

### Planned

- Disassembler
- Debugger
- Memory visualization

## 📦 Dependencies

| Dependency | Min. version |
| ---------- | ------------ |
| **CMake**  | 3.16         |
| **Qt**     | 6.x          |
| **C++**    | 17           |

### Installing Dependencies

<details>
<summary><b>Windows</b></summary>

1. Install [Qt 6](https://www.qt.io/download-qt-installer-oss) — during installation, select the **Qt Widgets** component.
2. Install [CMake](https://cmake.org/download/) (≥ 3.16) or use the one bundled with Qt.
3. A compiler supporting C++17: [Visual Studio 2019+](https://visualstudio.microsoft.com/) (MSVC) or [MinGW](https://www.mingw-w64.org/).

> [!TIP]
> When using Visual Studio, ensure that the "Desktop development with C++" workload is installed.

</details>

<details>
<summary><b>Linux (Ubuntu / Debian)</b></summary>

```bash
sudo apt update
sudo apt install cmake g++ qt6-base-dev
```

If the `qt6-base-dev` package is not available in your distribution, use the [official Qt installer](https://www.qt.io/download-qt-installer-oss).

</details>

<details>
<summary><b>macOS</b></summary>

Using [Homebrew](https://brew.sh/):

```bash
brew install cmake qt@6
```

</details>

## 🛠️ Build

### From Command Line

```bash
git clone https://github.com/igmunv/cremniy.git
cd cremniy

mkdir build
cd build
cmake ../src

cmake --build .
```

To build in Release mode:

```bash
cmake ../src -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 🤝 Contributing

Contributions are **welcome**.

All contributors will be added to [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)
and mentioned at the end of each video on the [YouTube channel](https://www.youtube.com/@igmunv).
