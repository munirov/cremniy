# KodoTerm

A free C++ Qt6 widget that embeds a terminal emulator using `libvterm`.

![KodoShell running on Linux](kodoshell-linux.png)
![KodoShell running on Windows](kodoshell-windows.png)

## Features

- **Simple Qt6 integration**: Derives `QWidget`, and contains somewhat
   compatibility with `QProcess`.
- **LibVTerm**: Uses the robust `libvterm` for VT100/xterm emulation.
- **PTY Support**: Handles pseudo-terminal on Windows and Unix.
- **Themes**: supports themes from [Konsole](https://github.com/KDE/konsole/tree/master/data/color-schemes)
  [WindowsTerminal]() and [iTerm](https://github.com/mbadolato/iTerm2-Color-Schemes).
- **Mouse integration**: Double/tripple click support (selection), clicks are 
  also sent to the applications.
- **Visual bell**: can make a sound, or blink the screen.

## Prerequisites

- C++20 compiler
- CMake 3.24+
- Qt6 (Core, Gui, Widgets)

> *Note for Windows developers*
>
> If you are developing an application that embeds this widget, you might need
> to run this application in a dedicated console (the debug messages will not
> be seen in the IDE, but the background window).
>
> The reason is that the communication between the console background process
> and the main application uses similar channels. This is a limitation of
> Windows, and not this library.
>
> As far as I know... pull-requests are welcomed....
>
> see [src/PtyProcess_win.cpp](src/PtyProcess_win.cpp) for implementation.

## Build

```bash
cmake -B build -G Ninja
cmake --build build
```

## Run Demo

```bash
./build/KodoTermSimple
./build/KodoShell
```

## License

MIT

## Known issues

1. It is rather slow. Specially on windows, painting takes too much time.
2. The demo has an option to re-display the main window with shortcuts, this
   is not working on Wayland.
3. It seems that `libvterm` is no longer maintained. Alternatives should be used
   eventually.
4. See [`todo.txt`](todo.txt) some tasks are not done yet.


## Author

Diego Iastrubni <diegoiast@gmail.com>
