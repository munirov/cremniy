# Cremniy — как писать плагины

Два слоя расширения:

1. **Настоящий плагин** — самодостаточная папка `plugins/<id>/` на верхнем уровне
   репо (рядом с `source/`), которая добавляет функционал **вкладами**
   (contributions), не правя код ядра. Это основной способ добавить фичу-единицу.
   См. раздел [«Настоящий плагин»](#настоящий-плагин-папка-plugins) ниже; эталон —
   `plugins/connections/`.
2. **Core-швы** — реестры внутри ядра (View-панели, tool-табы, меню, иконки,
   backend-команды…), которыми пользуются и ядро, и плагины. Полная карта — в
   разделе [«Полная карта точек расширения»](#полная-карта-точек-расширения).

Динамика (авто-дискавери папки, затем установка с сервера: гит/докер/поиск, в
перспективе **STM32-паки взамен тулчейна ST**) — цель; пока плагины локальны и
бандлятся на сборке. Не выдумывай удалённый загрузчик — регистрируйся в реестр.

Эталонный пример — пак **Source Control (git)**. По нему и идёт разбор; файлы:

- `source/backend/src/git.rs` — backend-команда `git_status`
- `source/frontend/src/infrastructure/tauri/bridge.ts` — мост `gitStatus`
- `source/frontend/src/boundary/workspace/GitPanel.tsx` (+ `.module.css`) — UI
- `source/frontend/src/boundary/workspace/SidePanel.tsx` — регистрация в реестре
- `source/frontend/src/boundary/workspace/activityBarIcons.tsx` — иконка

---

## Настоящий плагин: папка `plugins/`

Самодостаточный плагин живёт в **`plugins/<id>/`** на верхнем уровне репо (рядом с
`source/`), а не вшивается в код ядра. Он добавляет функционал только через
**декларативные вклады** — хост сам решает, куда их положить. Эталон —
`plugins/connections/` (менеджер хостов serial/SSH).

**Из чего состоит плагин:**
- `plugins/<id>/index.tsx` — `default`-экспорт манифеста `PluginManifest`
  (`@shared/plugins/contributions`): `{ id, name, centerPanels?, menuItems?, commands? }`.
- остальные файлы плагина рядом (компоненты, `.module.css`).
- регистрация: добавить плагин в список в `plugins/index.ts` (авто-дискавери —
  следующий шаг).

**Виды вкладов** (`shared/plugins/contributions.ts`):
- `centerPanels: [{ id, label, render }]` — вкладка в Рабочем поле.
- `menuItems: [{ menu: 'terminal' | 'tools', id, label, run }]` — пункт в меню хоста.
- `commands: [{ name, description, run }]` — команда агента/MCP (`window.cremniy`).

**Хост-API** (`@shared/plugins/host` → `pluginHost()`): то, что обработчик вклада
зовёт у живой IDE — пока `openPanel(id)` / `closePanel(id)`. Доступен к моменту
любого `run()` (его ставит `RootApp` на маунте), поэтому из манифеста зови его
лениво внутри `run`, а не на верхнем уровне.

**Как хост это подхватывает** (править НЕ нужно — уже разведено):
- `loadPlugins()` в `main.tsx` регистрирует все плагины из `plugins/` до маунта.
- `resolveCenterPanel(id)` (`centerPanels.tsx`) отдаёт core- ИЛИ плагин-панель.
- `MenuBar` дорисовывает `pluginMenuItems('terminal')` в меню.
- `RootApp` регистрирует `pluginCommands()` рядом с ядровыми командами.

**Ограничения v1 (локально, build-time):**
- плагины бандлятся на сборке (не с сервера ещё); список — в `plugins/index.ts`.
- плагин использует **react + алиасы хоста** (`@shared`, `@infrastructure`) + свой
  код; произвольные npm-зависимости пока не поддержаны. Vite резолвит react из
  фронта (`resolve.dedupe`), tsc — через `@plugins`-путь + маппинг react на `@types`.
- **тесты плагина** пока живут в `src` и импортируют панель через
  `@plugins/<id>/...` (вне-рутовый резолв тест-либ ещё не настроен).
- нативный backend плагина (Rust-команды) пока остаётся в `source/backend`. У
  Connections сессии serial/SSH рендерит **core-терминал** (`SerialInstance` /
  `SshInstance`), а плагин-панель дёргает их через шину `shared/connections/connectionBus`.

Остальные поверхности (View-панель, tool-таб, иконки, декорации…) плагин-вкладами
пока НЕ покрыты — это core-швы из карты ниже; контрибьюшн для них добавим по мере
надобности.

---

## Два вида паков

1. **View-пак** — панель в боковой панели (как Explorer / Search / Git). Живёт в
   реестре `VIEWS` в `SidePanel.tsx`, попадает в активити-бар и меню «Views» с
   пином. Используй для постоянных инструментов вокруг рабочей папки.
2. **Center-пак** — вкладка в **Рабочем поле**. *Рабочее поле* — это центральная
   зона с вкладками, где открываются файлы-редакторы (в коде — *center tab area*);
   она generic-хост: вкладкой может быть файл ИЛИ зарегистрированная **центр-панель**
   (*center panel*). Реестр центр-панелей — `CENTER_PANELS` в
   `boundary/layout/centerPanels.tsx`. Используй для того, что открывается «как
   документ» и чему тесно в узкой боковой колонке (Settings, Advanced Git, дифф,
   дашборд).

Обычный пак — это **UI (фронт)** + при необходимости **backend-команда (Rust)**.
Состояние, которое нужно пережить перезапуск, кладётся в `localStorage`
по ключу с workspace-путём (см. `treeView.ts`).

---

## Добавить view-пак (5 шагов, на примере git)

**1. Backend-команда (если паку нужны данные с диска / системы).**
Новый модуль `source/backend/src/<pack>.rs`:

```rust
use serde::Serialize;
use crate::canonical_workspace_root;

#[derive(Serialize)]
#[serde(rename_all = "camelCase")] // Rust snake_case → JS camelCase
pub struct GitStatus { /* … */ }

#[tauri::command]
pub fn git_status(workspace_root: String) -> Result<GitStatus, String> {
    let root = canonical_workspace_root(&workspace_root)?;
    // … вернуть данные или Err(String)
}
```

Правила:
- Возвращай `Result<T, String>`; ошибку отдавай строкой (фронт покажет).
- Пути наружу — через `crate::pretty_path(...)` (срезает виндовый `\\?\`, совпадает
  с форматом дерева, чтобы файлы открывались).
- Не тяни новый crate без нужды — это будит сетевую гонку индекса crates.io
  (см. `cremniy-auto-restart` память: сборка под `CARGO_REGISTRIES_CRATES_IO_PROTOCOL=sparse`).
  `git_status` обходится `std::process::Command` без зависимостей.
- «Нет данных» — это не ошибка: git-пак при «не репозиторий / git не в PATH»
  возвращает `is_repo: false`, а не `Err`, чтобы панель показала спокойный пустой стейт.

**2. Зарегистрируй команду** в `source/backend/src/lib.rs`:

```rust
// в generate_handler![ … ]
git::git_status,
// и рядом с остальными mod-ами
mod git;
```

**3. Мост на фронте** — `source/frontend/src/infrastructure/tauri/bridge.ts`:

```ts
export type GitStatus = { isRepo: boolean; /* … */ };

export async function gitStatus(workspaceRoot: string): Promise<GitStatus> {
  return invoke<GitStatus>("git_status", { workspaceRoot });
}
```

Имена полей — `camelCase` (совпадают с `rename_all` на Rust-структуре).

**4. Компонент панели** — `boundary/workspace/<Pack>Panel.tsx` (+ `.module.css`):

```tsx
export function GitPanel({ workspaceRoot }: { workspaceRoot: WorkspaceRoot | null }) {
  const { openFileFromWorkspace, fileTreeRevision } = useIdeSession();
  // грузим данные на mount / смену workspace / fileTreeRevision (он бампается
  // при create/delete/rename/save — так панель сама обновляется)
}
```

Стили — строго по [DESIGN.md](./DESIGN.md): моно-шрифт, приглушённый текст,
hover через `rgba(255,255,255,0.04)`, **никакого синего**. Скопируй заголовок/
стейты из `GitPanel.module.css` — они уже в каноне.

**5. Регистрация в реестре** — `boundary/workspace/SidePanel.tsx`:

```tsx
type ViewId = 'explorer' | 'search' | 'git';            // + id
const VIEWS = [ /* … */
  { id: 'git', label: 'Source Control', icon: <GitIcon size={17} /> },
];
// в теле:
) : active === 'git' ? (
  <GitPanel workspaceRoot={workspaceRoot} />
) : …
```

Иконка — монохромный `currentColor`-аутлайн на сетке 24px в `activityBarIcons.tsx`
(через хелпер `UiIcon`). Пин/меню «Views» подхватятся автоматически.

Готово: пак появится в активити-баре с пином, состоянием active и своим телом.

---

## Добавить center-пак (вкладку в Рабочем поле)

**Рабочее поле** — центральная зона с вкладками (там, где открываются файлы). Это
generic-хост: вкладкой может быть файл ИЛИ зарегистрированная **центр-панель**. Так
сделаны Settings и Advanced Git — большие экраны, которым тесно в боковой колонке.
Эталон — `AdvancedGitDialog` (Branches / Stash / History / Remotes).

**1. Запись в реестре** — `boundary/layout/centerPanels.tsx`:

```ts
export const CENTER_PANELS: Record<string, { label: string; render: () => ReactNode }> = {
  settings:    { label: 'Settings', render: () => <SettingsTab /> },
  advancedGit: { label: 'Git',      render: () => <AdvancedGitTab /> },
  // myTool:   { label: 'My Tool',  render: () => <MyToolTab /> },
};
```

`label` — текст вкладки, `render` — её тело, ключ (`id`) — то, чем открывают/закрывают.

**2. Тело вкладки** — маленький компонент-обёртка рядом, как `SettingsTab` / `AdvancedGitTab`:

```tsx
function AdvancedGitTab() {
  const workspaceRoot = useWorkspaceRoot();
  const { closePanel } = useIdeSession();
  return <AdvancedGitDialog embedded workspaceRoot={workspaceRoot?.path ?? null}
                            onClose={() => closePanel('advancedGit')} />;
}
```

Приём: один компонент — и модалка, и вкладка. Проп `embedded` убирает оверлей/Esc и
заполняет слот (см. `embedded` в `SettingsDialog` / `AdvancedGitDialog`). Размеры держи
в плотности chrome (12px-моно) и ограничивай ширину контента (`max-width`) — иначе на
всю ширину поля экран смотрится разреженным.

**3. Открыть / закрыть** — `useIdeSession().openPanel('advancedGit')` /
`closePanel('advancedGit')`. Табы и тело центра подхватят панель сами
(`IdeEditorTabStrip` / `IdeDockview`).

**4. (Опц.) команда-агент** — чтобы вкладка открывалась и кнопкой, и снаружи
(тест / ИИ / MCP), зарегистрируй в `RootApp.tsx` команду рядом с `dialog.openSettings`:

```tsx
{ name: 'dialog.openAdvancedGit', description: 'Open the Advanced Git tab.',
  run: () => ide.openPanel('advancedGit') }
```

Кнопка в UI зовёт ту же команду (`runAgentCommand('dialog.openAdvancedGit')`) — одна
логика, две двери (см. [AGENT_CONTROL.md](./AGENT_CONTROL.md)).

---

## Полная карта точек расширения

Всё, что пак может занять. ✅ — добавляется записью в реестр / вызовом хука;
⚠️ — есть, но правится точечно в коде; ❌ — шва пока нет (цель — не выдумывай загрузчик).

| # | Шов | Файл | Как добавить | |
|---|-----|------|--------------|---|
| 1 | View (боковая панель) | `boundary/workspace/SidePanel.tsx` | запись в `VIEWS` + ветка рендера + иконка | ✅ |
| 2 | Центр-панель (вкладка в Рабочем поле) | `boundary/layout/centerPanels.tsx` | запись в `CENTER_PANELS` + `openPanel(id)` | ✅ |
| 3 | Tool-таб (нижний док) | `domain/toolTabs/toolTabCatalog.ts` | id в `TOOL_TAB_IDS` + запись в каталог + ветка в `IdeToolDock.tsx` | ✅ |
| 4 | Команды агента | `shared/agent/agentBridge.ts` | `registerAgentCommands([...])` в эффекте | ✅ |
| 5 | Состояние агента | `shared/agent/agentBridge.ts` | `registerAgentState(key, () => snapshot)` | ✅ |
| 6 | MCP-инструмент | `backend/src/mcp.rs` | запись в `tool_defs()` + ветка в `call_tool` (обычно мост в команду агента) | ✅ |
| 7 | Backend-команда | `backend/src/lib.rs` | `#[tauri::command]` + `generate_handler!` + мост в `bridge.ts` | ✅ |
| 8 | Иконка активити-бара | `boundary/workspace/activityBarIcons.tsx` | новый `currentColor`-SVG компонент | ✅ |
| 9 | Иконки типов файлов | `boundary/workspace/fileicons/` | SVG в `icons/` + запись в `theme.json` | ✅ |
| 10 | Pane (вынос панели в окно) | `boundary/layout/paneRegistry.ts` | `registerPaneRenderer(id, () => <Pane/>)` | ✅ |
| 11 | Пункт меню | `domain/menu/<x>Menu.ts` + `chrome/MenuBar.tsx` | id в union + запись + диспатч в `RootApp` | ✅ |
| 12 | Тосты / уведомления | `boundary/notifications/NotificationContext.tsx` | хук `useNotify()` | ✅ |
| 13 | Префы (per-user) | `domain/preferences/appPreferences.ts` | поле в `AppPreferences` + save/load | ✅ |
| 14 | Сессия проекта (`.cremniy`) | `domain/project/cremniyMeta.ts` | поле в `CremniySessionState` + read/write meta | ✅ |
| 15 | localStorage (per-workspace) | `boundary/workspace/treeView.ts` (паттерн) | свой ключ `cremniy.<feature>:<root>` | ✅ |
| 16 | Декорации дерева (git-точки) | `boundary/workspace/gitDecorations.ts` | правится в коде (`GitDecoKind` / `fileDeco`) | ⚠️ |
| 17 | Хоткеи | `domain/menu/menuShortcuts.ts` | правится в `matchShortcutAction` (реестра нет) | ⚠️ |
| 18 | Меню Build | — | заглушка-плейсхолдер, шва ещё нет | ❌ |
| 19 | Статус-бар | — | компонента нет | ❌ |

Группами: **UI** — 1, 2, 3, 8, 9, 10, 12; **управление / агент** — 4, 5, 6 (одна поверхность,
см. [AGENT_CONTROL.md](./AGENT_CONTROL.md): команда сначала, кнопка/MCP зовут её); **backend** —
7; **меню / ввод** — 11, 16, 17; **состояние** — 13 (глобальное), 14 (на проект), 15 (мелочь
на воркспейс).

Типичный пак занимает несколько швов сразу. Source Control (git) = backend-команды (7) +
мост + View-панель (1) + центр-панель «Advanced Git» (2) + декорации (16) + команды агента
(4) + иконки (8, 9). Это и есть «расширить всё»: один пак, разные двери, без правки ядра
(кроме записи в реестры).

## Чеклист перед коммитом пака

- `npx tsc --noEmit` из `source/frontend` — без новых ошибок (база известна).
- `cargo check` из `source/backend` под `CARGO_REGISTRIES_CRATES_IO_PROTOCOL=sparse`,
  если трогал Rust.
- Тесты рядом, где есть логика без UI (движок — как `nesting.test.ts`).
- Стиль — сверён с [DESIGN.md](./DESIGN.md).
- Коммит гранулярный, сухой `type(scope): что` (см. `cremniy-commit-style`).

---

## Куда это растёт

Сделано: реальные плагины в `plugins/` с реестром вкладов (локально, build-time);
Connections мигрирован туда первым плагином. Дальше по порядку:
1. **авто-дискавери** папки (убрать ручной список в `plugins/index.ts`);
2. **тесты + произвольные npm-deps** внутри плагина (вне-рутовый резолв);
3. **backend плагина** как сайдкар/WASM (сейчас Rust-команды живут в ядре);
4. **манифест + загрузка с сервера**: ide спрашивает реестр → качает папку → ставит;
   так встанут git/docker/поиск, а в перспективе **STM32-паки взамен тулчейна ST**.

Текущая граница (вклады `contributions` + `@shared/plugins` + `bridge`, плюс
core-швы `VIEWS` / `CENTER_PANELS` / `#[tauri::command]`) — то, за что динамический
загрузчик встанет, не ломая уже написанные плагины.
