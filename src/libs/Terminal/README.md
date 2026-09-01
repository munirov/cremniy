# Cremniy Terminal

Cross-platform terminal module used by Cremniy.

## Origin

The terminal emulator and view layer are adapted from the standalone Terminal
solution in [Qt Creator](https://github.com/qt-creator/qt-creator):

- upstream directory: [`src/libs/solutions/terminal`](https://github.com/qt-creator/qt-creator/tree/b815fca1aa9ab2332de8968609799290e9cd73cf/src/libs/solutions/terminal)
- imported revision: [`b815fca1aa9ab2332de8968609799290e9cd73cf`](https://github.com/qt-creator/qt-creator/commit/b815fca1aa9ab2332de8968609799290e9cd73cf)

Cremniy keeps an adapted copy in-tree together with its own PTY integration so
the terminal can be built reproducibly on Windows, Linux, and macOS.

## Licenses and third-party components

This directory contains code under several licenses. The license declared in
each source file is authoritative.

| Component | Source | License |
| --- | --- | --- |
| Qt Creator Terminal solution | [qt-creator/qt-creator](https://github.com/qt-creator/qt-creator/tree/b815fca1aa9ab2332de8968609799290e9cd73cf/src/libs/solutions/terminal) | GPL-3.0 with The Qt Company GPL Exception 1.0; the original headers also offer Qt commercial licensing |
| Scrollback implementation | [jsbronder/sff](https://github.com/jsbronder/sff) | BSD-3-Clause |
| libvterm 0.3.3 | [libvterm upstream](https://www.leonerd.org.uk/code/libvterm/) | MIT |
| Cremniy integration and PTY backends | [munirov/cremniy](https://github.com/munirov/cremniy) | GPL-3.0 |

The corresponding license texts are included here:

- [`LICENSE`](LICENSE) — GNU GPL version 3
- [`LICENSE.Qt-GPL-exception-1.0`](LICENSE.Qt-GPL-exception-1.0) — The Qt Company GPL Exception 1.0
- [`LICENSE.BSD-3-Clause`](LICENSE.BSD-3-Clause) — scrollback implementation
- [`third_party/libvterm/LICENSE`](third_party/libvterm/LICENSE) — libvterm MIT license

See [`UPSTREAM.md`](UPSTREAM.md) for the concise provenance record.
