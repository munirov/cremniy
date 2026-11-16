# Terminal core provenance

The terminal emulator is adapted from Qt Creator's standalone Terminal
solution:

- repository: <https://github.com/qt-creator/qt-creator>
- upstream directory: `src/libs/solutions/terminal`
- imported revision: [`b815fca1aa9ab2332de8968609799290e9cd73cf`](https://github.com/qt-creator/qt-creator/commit/b815fca1aa9ab2332de8968609799290e9cd73cf)

That Qt Creator revision vendors libvterm 0.3.3. Cremniy keeps the required
libvterm sources under `third_party/libvterm`.

The directory contains code under multiple licenses:

- Qt Creator Terminal sources: GPL-3.0 with The Qt Company GPL Exception 1.0
- scrollback implementation from <https://github.com/jsbronder/sff>:
  BSD-3-Clause
- libvterm: MIT
- Cremniy integration and PTY backends: GPL-3.0

See [`README.md`](README.md) for direct source links and the complete license
file map.
