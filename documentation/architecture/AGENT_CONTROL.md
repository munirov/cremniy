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
- `file.*` / `session.*` — открытие/сохранение, вкладки, активный документ.
- `view.*` / `tool.*` / `edit.*` / `dialog.*` — IDE chrome: панели, инструменты, поиск, диалоги.
- `fs.*` — операции с файлами workspace по явным аргументам: `list`, `readText`, `readBytes`,
  `createFile`, `writeText`, `writeBytes`, `createFolder`, `rename`, `delete`. `writeBytes`
  принимает `number[]` или hex-строку (`"deadbeef"`).
- `process.*` — запуск программ:
  - `run { program, args?, relativeCwd?, timeoutMs? }` — stdout/stderr/exit, таймаут, лимит вывода.
  - `build { source, output? }` — обёртка `rustc build`.

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

## Карта кода

| Файл | Роль |
|---|---|
| `source/frontend/src/shared/agent/agentBridge.ts` | Реестр + API `window.cremniy`. |
| `source/frontend/src/boundary/agent/AgentWorkspaceCommands.tsx` | `fs.*` / `process.*`. |
| `source/frontend/src/boundary/RootApp.tsx` | IDE chrome + state `ui`. |
| `source/frontend/src/boundary/workspace/IdeSessionContext.tsx` | `file.*` / `session.*` + state `session`. |
| `source/frontend/src/boundary/welcome/WelcomeView.tsx` | `welcome.*`. |
| `source/backend/src/process.rs` | Хардненный раннер процессов. |
