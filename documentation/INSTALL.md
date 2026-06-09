<div align="center">

**Language / Язык:** open one section below (only one stays open in modern browsers).

</div>

# Install, data & uninstall

<details name="install-lang" open>
<summary><strong>English</strong></summary>

## Install

Download the installer for your OS from the **[latest release](https://github.com/munirov/cremniy/releases/latest)**. The per-OS files (`.exe`/`.msi`, `.dmg`, `.AppImage`/`.deb`) and the first-launch notes for unsigned builds are listed in the **[Download section of the README](../README.md#download)**.

Prefer to build it yourself? See the **[developer guide](EN/developer_guide.md)**.

## Where Cremniy stores your data

Cremniy runs **fully locally** — no account, no telemetry, nothing is sent anywhere. All your settings live in **one folder**, named after the app identifier `com.cremniy.app`:

| OS | Folder |
|----|--------|
| **Windows** | `%APPDATA%\com.cremniy.app\` &nbsp;(`C:\Users\<you>\AppData\Roaming\com.cremniy.app\`) |
| **macOS** | `~/Library/Application Support/com.cremniy.app/` |
| **Linux** | `~/.config/com.cremniy.app/` |

Inside that folder:

- **`preferences.json`** — UI & tool settings: theme, language, recently-opened folders, editor (font size, word wrap, indentation), file-tree hide patterns, hex-view layout, disassembler configuration, and saved panel sizes.
- **`connections.json`** — saved **Connections** hosts (serial / SSH / SFTP). May contain host credentials — treat it as sensitive.
- **`terminal_history.txt`** — integrated-terminal command history.

The built-in WebView also keeps a small local cache (enabled/disabled plugins, file-tree order, unpinned side-panel views) in the app's data area — on Windows that's a separate `%LOCALAPPDATA%\com.cremniy.app\` folder.

Your **project files are never copied here** — Cremniy edits them in place, wherever they live on disk.

## Uninstall

| OS | How |
|----|-----|
| **Windows** | Settings → Apps → installed apps → **Cremniy** → Uninstall. (Or run the bundled uninstaller / remove it from "Installed apps" if you used the `.msi`.) |
| **macOS** | Open **Applications**, drag **Cremniy** to the Trash. |
| **Linux** | `.deb`: `sudo apt remove cremniy` (or `sudo dpkg -r cremniy`). AppImage: delete the `Cremniy_*.AppImage` file. |

Uninstalling removes the program but **leaves your settings folder** in place, so a reinstall keeps your setup. To remove your data too, delete the per-OS folder listed above (on Windows, also delete `%LOCALAPPDATA%\com.cremniy.app\`).

</details>

<details name="install-lang">
<summary><strong>Русский</strong></summary>

## Установка

Скачайте установщик для вашей ОС со **[страницы последнего релиза](https://github.com/munirov/cremniy/releases/latest)**. Файлы под каждую ОС (`.exe`/`.msi`, `.dmg`, `.AppImage`/`.deb`) и заметки про первый запуск неподписанных сборок — в **[разделе «Скачать» в README](../README.md#скачать)**.

Хотите собрать сами — см. **[руководство разработчика](RU/developer_guide.md)**.

## Что Cremniy хранит на диске

Cremniy работает **полностью локально** — без аккаунта, без телеметрии, ничего никуда не отправляется. Все настройки лежат в **одной папке**, названной по идентификатору приложения `com.cremniy.app`:

| ОС | Папка |
|----|-------|
| **Windows** | `%APPDATA%\com.cremniy.app\` &nbsp;(`C:\Users\<вы>\AppData\Roaming\com.cremniy.app\`) |
| **macOS** | `~/Library/Application Support/com.cremniy.app/` |
| **Linux** | `~/.config/com.cremniy.app/` |

Внутри:

- **`preferences.json`** — настройки интерфейса и инструментов: тема, язык, недавние папки, редактор (размер шрифта, перенос строк, отступы), маски скрытия в дереве файлов, раскладка hex-просмотра, конфиг дизассемблера и сохранённые размеры панелей.
- **`connections.json`** — сохранённые хосты **Connections** (serial / SSH / SFTP). Может содержать учётные данные — относитесь как к конфиденциальному.
- **`terminal_history.txt`** — история команд встроенного терминала.

Встроенный WebView также держит небольшой локальный кэш (включённые/выключенные плагины, порядок дерева файлов, откреплённые виды боковой панели) в области данных приложения — на Windows это отдельная папка `%LOCALAPPDATA%\com.cremniy.app\`.

**Файлы ваших проектов сюда не копируются** — Cremniy правит их на месте, где бы они ни лежали.

## Удаление

| ОС | Как |
|----|-----|
| **Windows** | Параметры → Приложения → установленные приложения → **Cremniy** → Удалить. (Либо запустите встроенный деинсталлятор / удалите через «Установленные приложения», если ставили `.msi`.) |
| **macOS** | Откройте **Программы**, перетащите **Cremniy** в Корзину. |
| **Linux** | `.deb`: `sudo apt remove cremniy` (или `sudo dpkg -r cremniy`). AppImage: удалите файл `Cremniy_*.AppImage`. |

Удаление убирает саму программу, но **оставляет папку настроек** — чтобы при переустановке всё сохранилось. Чтобы стереть и данные, удалите папку для вашей ОС из таблицы выше (на Windows ещё и `%LOCALAPPDATA%\com.cremniy.app\`).

</details>
