# Агентская поверхность

Сквозное правило Cremniy: **каждая UI-способность сначала команда, потом кнопка.**

## Правило

- Любое действие, доступное человеку кликом или хоткеем, — именованная команда в
  `window.cremniy.run('area.action', args?)`.
- Любая часть экрана, которую человек видит, опубликована полем в `window.cremniy.state()`.
- Кнопка в UI и внешний клиент (скрипт, тест, ИИ) дёргают **одну и ту же функцию**.

Зачем: одна логика, одни данные, две двери. Скриптуемость без скрейпинга DOM, стабильная
поверхность для тестов, управление со стороны ИИ-агента — бесплатно при добавлении любой
способности.

## API

`window.cremniy` ставится в `main.tsx` до монтирования. Поверхность регистрируют компоненты при
монтировании.

| Поле | Описание |
|---|---|
| `version` | Версия поверхности (сейчас `1`). |
| `commands()` | `{ name, description }[]` — что доступно на текущем экране. |
| `state()` | Снимок того, что видит пользователь, по областям (`ui`, `session`, …). |
| `run(name, args?)` | Вызвать команду; промис с результатом. Неизвестное имя — реджект с подсказкой `commands()`. |

## Группы команд

Имена namespaced по области. Welcome и IDE регистрируют разные наборы — `commands()` это
отражает.

- `welcome.*` — экран приветствия (открыть проект, недавние).
- `file.*` / `session.*` — открытие/сохранение, вкладки, активный документ, сплит-группы
  редактора (`session.splitActiveFile { side? }`, `session.moveFileToGroup`, `session.activateGroup`).
- `view.*` / `tool.*` / `edit.*` / `dialog.*` — IDE chrome: панели, инструменты, поиск, диалоги.
- `plugin.*` — управление плагинами: `list`, `setEnabled { id, enabled }`, `showDetails { id }`.
- `fs.*` — операции с файлами workspace по явным аргументам: `list`, `readText`, `readBytes`,
  `createFile`, `writeText`, `writeBytes`, `createFolder`, `rename`, `delete`. `writeBytes`
  принимает `number[]` или hex-строку (`"deadbeef"`).
- `process.*` — запуск программ:
  - `run { program, args?, relativeCwd?, timeoutMs? }` — stdout/stderr/exit, таймаут, лимит вывода.
  - `build { source, output? }` — обёртка над `rustc <source> -o <output>`.
- `terminal.*` — чтение терминала (read-only, без изменения UI):
  - `read { lines? }` — весь скроллбэк активного терминала текстом (не только видимое); `lines`
    оставляет последние N строк. Возврат: `{ sessionIndex, label, terminalCount, lineCount, text }`.
  - `list {}` — открытые терминалы (`index`, `label`, активный, читается ли буфер).

## Примеры

```js
// Навигация и состояние
await cremniy.run('welcome.openFolder');
cremniy.state();                                // { ui: { route: 'ide', ... } }
await cremniy.run('tool.select', { id: 'binary' });
await cremniy.run('session.openFile', { path: '/abs/path/file.bin' });
cremniy.state().session;                        // активный файл, вкладки, dirty-флаги, текст

// Edit → build → run
await cremniy.run('fs.writeText', { path: 'hello.rs', text: 'fn main(){ println!("hi"); }' });
const build = await cremniy.run('process.build', { source: 'hello.rs' });
const out = await cremniy.run('process.run', { program: './hello' });   // out.stdout === "hi\n"
```

## Правило для кода

Логика идёт в команду, кнопка её зовёт. Регистрация — в компоненте, который и так владеет
поведением:

```ts
useEffect(() => registerAgentCommands([
  { name: 'area.action', description: 'Что делает { args }.', run: (a) => doThing(a) },
]), []);
```

Чтобы регистрация-эффект отработала один раз, но отражала текущий экран — читай актуальные
значения через ref внутри `run` и продьюсеров состояния. См. `RootApp.tsx`,
`IdeSessionContext.tsx`, `WelcomeView.tsx`, `boundary/agent/AgentWorkspaceCommands.tsx`.

Что **нельзя**:
- класть логику внутрь `onClick` напрямую (команды либо нет, либо она дублируется);
- регистрировать команду в одном месте, а в кнопке звать функцию из другого;
- читать состояние через DOM или прямой импорт хранилища — только через `state()` (для команд) и
  хуки `_hooks/` (для UI).

## Безопасность

`fs.*` и `process.*` идут через Tauri IPC — только в десктопе (`npm run tauri:dev` /
`tauri:build`), не в browser preview. Нативный раннер (`source/backend/src/process.rs`):

- рабочая директория обязана резолвиться **внутри** workspace;
- аргументы — явный argv, без shell (никаких shell-инъекций);
- wall-clock timeout с watchdog, убивающим дерево процессов;
- лимит по объёму вывода на поток.

Голое имя программы (например, `hello`), существующее в рабочей директории, резолвится до
абсолютного пути, чтобы build-then-run работал на всех ОС; PATH-инструменты (например, `cargo`)
остаются как есть.

## MCP-сервер (внешний доступ)

`window.cremniy` живёт внутри webview. Чтобы тем же управлял внешний агент (ИИ),
приложение поднимает **MCP-сервер прямо в себе**: HTTP на `127.0.0.1:41547/mcp`
(стартует в `setup()` бэкенда — `source/backend/src/mcp.rs`).

Адрес фиксированный — `http://127.0.0.1:41547/mcp` — так что дёргать сервер можно прямо по
HTTP (любой POST с JSON-RPC), отдельный конфиг не нужен. `.mcp.json` нужен **только** если
хочется видеть инструменты как «родные» MCP-инструменты клиента (тип `http`); это опционально:

```json
{ "mcpServers": { "cremniy": { "type": "http", "url": "http://127.0.0.1:41547/mcp" } } }
```

Инструменты:

| Tool | Что делает |
|---|---|
| `list_commands` | как `window.cremniy.commands()` — команды текущего экрана. |
| `run_command` `{ name, args? }` | как `window.cremniy.run(...)` — дёрнуть любую UI-команду. |
| `get_state` | как `window.cremniy.state()` — снимок состояния экрана. |
| `list_windows` | окна приложения (главное + вынесенные панели). |
| `screenshot` `{ label? }` | PNG каждого окна (или по label) — **webview сам рендерит свой DOM в PNG через `html-to-image`, а не снимок экрана; поэтому работает даже когда окно свёрнуто/перекрыто/не на переднем плане** (в кадр попадает только то, что реально в DOM). |

Поток: HTTP → бэкенд эмитит `agent://request` в webview → слушатель
(`shared/agent/agentRemote.ts`) зовёт `window.cremniy` → ответ командой `agent_reply` →
HTTP-ответ. Второй реализации команд нет: MCP — тонкий внешний мост к той же поверхности,
что и кнопки. Скриншоты — webview рендерит свой `document.documentElement` в PNG через `html-to-image` (`toPng` в `agentRemote.ts`); мост — тот же `agent://request` (kind `capture`).

## Два режима управления (UI-тихий / UI-видимый)

Одна поверхность — два стиля, выбирай под задачу:

1. **UI-тихий (headless)** — команда/инструмент возвращает данные, **интерфейс не трогается**.
   Пример: прочитать консоль целиком — `terminal.read` отдаёт весь скроллбэк (хоть 1000 строк).
   Сюда же `get_state`, `fs.readText`, `list_commands`. Быстро, полно, на экране ничего не дёргается.
2. **UI-видимый** — `run_command` дёргает реальную кнопку, и это **видно** (открыть файл
   `session.openFile`, переключить вид, `dialog.openAdvancedGit`, `tool.select`). Нужно, когда
   важно само состояние UI или чтобы пользователь видел действие.

Про **`screenshot`**: он ловит только то, что реально нарисовано — **видимые** строки, не весь
буфер. Поэтому «прочитать всё» → команда-чтение (тип 1); скриншот → когда нужен именно визуал.
Данных больше, чем влезает (длинный лог)? Варианты: (а) команда-чтение целиком; (б) развернуть/
увеличить панель — UI-видимый шаг — и затем снять скрин: в кадр попадёт больше строк (сколько —
зависит от высоты окна).

## Карта кода

| Файл | Роль |
|---|---|
| `source/frontend/src/shared/agent/agentBridge.ts` | Реестр + API `window.cremniy`. |
| `source/frontend/src/shared/agent/agentRemote.ts` | Мост MCP-сервер ↔ `window.cremniy`. |
| `source/backend/src/mcp.rs` | MCP HTTP-сервер + захват окон. |
| `source/frontend/src/boundary/agent/AgentWorkspaceCommands.tsx` | `fs.*` / `process.*`. |
| `source/frontend/src/boundary/RootApp.tsx` | IDE chrome + state `ui`. |
| `source/frontend/src/boundary/workspace/SidePanel.tsx` | `view.*` (список / выбор / закрепление панелей). |
| `source/frontend/src/boundary/workspace/IdeSessionContext.tsx` | `file.*` / `session.*` + state `session`. |
| `source/frontend/src/boundary/welcome/WelcomeView.tsx` | `welcome.*`. |
| `source/frontend/src/boundary/terminal/TerminalFooterPanel.tsx` | `terminal.*` (читает буфер активного xterm). |
| `source/backend/src/process.rs` | Хардненный раннер процессов. |
