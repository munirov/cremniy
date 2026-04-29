<div align="center">

<img src="docs/cremniy_icon_stroke.svg" width="250" alt="Cremniy logo">

<br>
<h3>Cremniy</h3>
<h6>All tools for low-level development are combined and linked in a single application — write code, edit bytes, and analyze binaries without extra windows</h6>

[![License](https://img.shields.io/github/license/igmunv/cremniy?color=orange&style=flat-square)](LICENSE)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)
<br>
[![Tauri](https://img.shields.io/badge/Tauri-2-24C8D8?style=flat-square&logo=tauri)](https://tauri.app/)
[![React](https://img.shields.io/badge/React-18-61DAFB?style=flat-square&logo=react)](https://react.dev/)

English • [Русский](README_ru.md)

</div>

<br>

## Desktop app (Tauri + React)

The in-repo desktop product is **Tauri 2 + React + TypeScript** under [`frontend/`](frontend/). See [ADR-001](ai_docs/develop/architecture/ADR-001-tauri-desktop-primary.md) and [ADR-002](ai_docs/develop/architecture/ADR-002-qt-sources-removed.md).

**Development:**

```bash
cd frontend
npm install
npm run dev
npm run tauri:dev
```

**Production build (local):**

```bash
cd frontend
npm run tauri:build
```

Installers and bundles are written to `frontend/src-tauri/target/release/bundle/`.

The legacy **Qt/C++** IDE has been **removed** from this repository. The last revision that still contained the `src/` tree is tagged **`pre-qt-removal-2026-05-01`** (clone and `git checkout` that tag if you need the old sources for reference).

---

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

Early **Tauri** builds focus on shell, workspace flow, and parity with the former welcome/IDE chrome. Deep tool parity (hex, disassembly UI, integrated terminal, etc.) is tracked in [ROADMAP.md](ROADMAP.md) and `ai_docs/`.

### Coming soon

- Full **HEX editor** and binary views in the web UI
- **Disassembler** UI backed by the Rust subprocess contract
- **Debugger** — step through execution, inspect registers and memory
- **Memory visualization** — visual maps of memory layout and allocation

## Contributing

Contributions are **welcome and encouraged**.

Whether it's a bug fix, a new feature, or an improvement to documentation — feel free to open an issue or submit a pull request.

All contributors are credited in [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) and mentioned in videos on the [YouTube channel](https://www.youtube.com/@igmunv).

For guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Distributed under the terms described in [LICENSE](LICENSE).
