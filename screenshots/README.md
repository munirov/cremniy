# Screenshots

A visual tour of Cremniy — an integrated environment for low-level and
reverse-engineering work. Every tool lives in one window, binaries open straight
from the file tree, and the feature set is built from plugins. · Визуальный тур
по Cremniy.

---

### 1. One window, every tool · Все инструменты в одном окне

![Overview](01-overview.png)

Code editor, file tree, editor tabs and an integrated terminal in a single
window — the whole low-level workflow without switching between separate apps.

> Редактор кода, дерево файлов, вкладки и встроенный терминал в одном окне —
> весь низкоуровневый процесс без переключения между приложениями.

---

### 2. Hex / binary editor · HEX / бинарный редактор

![Hex editor](02-hex.png)

Open any file as bytes — side-by-side hex and ASCII panes, undo/redo and patch
export. The format panel recognises common containers (ELF, PE, MBR) or falls
back to a raw byte stream.

> Откройте любой файл как байты — hex- и ASCII-панели рядом, undo/redo и экспорт
> патчей. Панель формата распознаёт ELF, PE, MBR или показывает сырой поток.

---

### 3. Disassembler · Дизассемблер

![Disassembler](03-disasm.png)

Disassemble an executable into annotated x86 / x64 — sections, function labels,
string references and inline byte patching. Runs on an embedded `iced-x86`
engine, so no external toolchain is required.

> Дизассемблирование исполняемого файла в x86 / x64 — секции, метки функций,
> строковые ссылки и патчинг байтов. Работает на встроенном движке `iced-x86`,
> внешний тулчейн не нужен.

---

### 4. Source control, built in · Встроенный контроль версий

![Git](04-git.png)

The Git plugin opens as a center tab — local and remote branches, stash,
history and merge/rebase, working against a real repository.

> Плагин Git открывается центральной вкладкой — локальные и удалённые ветки,
> stash, история, merge/rebase на настоящем репозитории.

---

### 5. Everything is a plugin · Всё — плагин

![Extensions](05-extensions.png)

Connections, Source Control and Binary Tools are plugins. Enable or disable any
of them from the Extensions panel — the UI appears or disappears live, with no
reload.

> Connections, Source Control и Binary Tools — это плагины. Включайте или
> выключайте любой из панели Extensions — интерфейс появляется и исчезает сразу,
> без перезапуска.

---

### 6. A page for every plugin · Страница у каждого плагина

![Plugin details](06-plugin-details.png)

Each plugin has its own details tab — description, the panels and commands it
contributes, version, author and links — rendered from the plugin's manifest.

> У каждого плагина своя вкладка с описанием — какие панели и команды он
> добавляет, версия, автор и ссылки — из манифеста самого плагина.

---

### 7. Connections · Подключения

![Connections](07-connections.png)

Save serial, SSH and SFTP hosts and open them as terminal tabs — part of the
Connections plugin.

> Сохраняйте serial-, SSH- и SFTP-хосты и открывайте их вкладками терминала —
> плагин Connections.

---

### 8. SFTP file transfer · Передача файлов по SFTP

![SFTP](08-sftp.png)

A dual-pane local ↔ remote file browser over SSH — upload and download without
leaving the IDE.

> Двухпанельный обзор «локально ↔ удалённо» поверх SSH — загрузка и скачивание
> не выходя из IDE.
