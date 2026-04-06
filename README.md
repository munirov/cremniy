<div align="center">

<img src="docs/cremniy_icon_stroke.svg" width="250" alt="Cremniy logo">

<br>
<h3>Cremniy</h3>
<h6>All tools for low-level development are combined and linked in a single application — write code, edit bytes, and analyze binaries without extra windows</h6>

[![License](https://img.shields.io/github/license/igmunv/cremniy?color=orange&style=flat-square)](LICENSE)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)
<br>
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square&logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![Qt 6](https://img.shields.io/badge/Qt-6.x-41CD52?style=flat-square&logo=qt)](https://www.qt.io/)

English • [Русский](README_ru.md)

</div>

<br>

## What is Cremniy?

**Cremniy** is an integrated environment for low-level development. Instead of keeping a HEX editor in one window, a disassembler in another, and a code editor in a third — all tools are combined and linked in a single convenient application.

**Designed for:**

- 🛠 System software developers
- 🔍 Reverse engineers
- 🔐 Cybersecurity specialists
- 📡 Embedded systems developers

## Why Cremniy?

Low-level development today means using a code editor, HEX editor, disassembler, debugger, all opened **in separate windows**.

You constantly **switch** between different windows, and the tools are **not linked** together.

#### **Cremniy solves this!**
- 🔘 Everything is in one place
- 🔗 All tools are connected
- 💻 Unified workflow

![out](https://github.com/user-attachments/assets/f5e9c520-fb31-45cc-ab11-17eff66d7069)

## Features

### Available now

| Feature | Description |
|---|---|
| 📝 Code editor | Write and edit low-level code with syntax support |
| 🔢 HEX editor | Inspect and modify binary data at the byte level (patching) |
| 🔧 Disassembler | Decode machine instructions into readable assembly |

### Coming soon

- 🐛 **Debugger** — step through execution, inspect registers and memory
- 🧠 **Memory visualization** — visual maps of memory layout and allocation

## Build

### Prerequisites

| Dependency | Minimum version |
|---|---|
| **CMake** | 3.16 |
| **Qt** | 6.x |
| **C++ compiler** | C++17 support |

<details>
<summary><b>🪟 Windows</b></summary>

1. Install [MSYS2](https://www.msys2.org/)
2. Install MinGW, CMake, Qt6-base via **MSYS2 terminal**:
```base
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-qt6-base
```
3. Add MSYS2 package directory to PATH  
   MSYS2 packages are located in `C:\msys64\ucrt64\bin` by default.

</details>

<details>
<summary><b>🐧 Linux (Ubuntu / Debian)</b></summary>

```bash
sudo apt update
sudo apt install cmake g++ qt6-base-dev
```

> [!NOTE]
> If `qt6-base-dev` is unavailable in your distribution's repositories, use the [official Qt installer](https://www.qt.io/download-qt-installer-oss) instead.

</details>

<details>
<summary><b>🍎 macOS</b></summary>

Using [Homebrew](https://brew.sh/):

```bash
brew install cmake qt@6
```

</details>

### Build in Linux

```bash
git clone https://github.com/igmunv/cremniy.git
cd cremniy

mkdir build && cd build
cmake ../src
cmake --build .
```

#### Release build

```bash
cmake ../src -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Build in Windows

```bash
git clone https://github.com/igmunv/cremniy.git
cd cremniy

mkdir build && cd build
cmake -G "MinGW Makefiles" ..\src
cmake --build .

```

#### Release build

```bash
cmake ..\src -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Contributing

Contributions are **welcome and encouraged**.

Whether it's a bug fix, a new feature, or an improvement to documentation — feel free to open an issue or submit a pull request.

All contributors are credited in [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) and mentioned in videos on the [YouTube channel](https://www.youtube.com/@igmunv).

For guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Distributed under the terms described in [LICENSE](LICENSE).
