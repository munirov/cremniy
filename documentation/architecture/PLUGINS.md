# Cremniy — как писать паки (плагины)

Как добавить новую функциональную единицу — «пак» — в Cremniy. Документ описывает
**текущий** механизм: статический реестр-шов (паки регистрируются в коде на этапе
сборки). Динамическая загрузка паков (гит/докер/STM32 как устанавливаемые
расширения) — это цель, к ней шов и проектируется, но пока её нет; не выдумывай
динамический загрузчик, регистрируйся в существующий реестр.

Эталонный пример — пак **Source Control (git)**. По нему и идёт разбор; файлы:

- `source/backend/src/git.rs` — backend-команда `git_status`
- `source/frontend/src/infrastructure/tauri/bridge.ts` — мост `gitStatus`
- `source/frontend/src/boundary/workspace/GitPanel.tsx` (+ `.module.css`) — UI
- `source/frontend/src/boundary/workspace/SidePanel.tsx` — регистрация в реестре
- `source/frontend/src/boundary/workspace/activityBarIcons.tsx` — иконка

---

## Два вида паков

1. **View-пак** — панель в боковой панели (как Explorer / Search / Git). Живёт в
   реестре `VIEWS` в `SidePanel.tsx`, попадает в активити-бар и меню «Views» с
   пином. Используй для постоянных инструментов вокруг рабочей папки.
2. **Center-пак** — вкладка в центральном пространстве (как Settings). Живёт в
   реестре `CENTER_PANELS` в `boundary/layout/centerPanels.tsx`. Используй для
   того, что открывается «как документ» (редактор, дифф, дашборд).

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

## Добавить center-пак (вкладку)

`boundary/layout/centerPanels.tsx` — добавь запись в `CENTER_PANELS`:

```ts
export const CENTER_PANELS: Record<string, { label: string; render: () => ReactNode }> = {
  settings: { label: 'Settings', render: () => <SettingsTab /> },
  // myTool: { label: 'My Tool', render: () => <MyToolTab /> },
};
```

Открыть вкладку из кода — `useIdeSession().openPanel('myTool')`. Табы и тело
центра подхватят её сами (см. `IdeEditorTabStrip` / `IdeDockview`).

---

## Чеклист перед коммитом пака

- `npx tsc --noEmit` из `source/frontend` — без новых ошибок (база известна).
- `cargo check` из `source/backend` под `CARGO_REGISTRIES_CRATES_IO_PROTOCOL=sparse`,
  если трогал Rust.
- Тесты рядом, где есть логика без UI (движок — как `nesting.test.ts`).
- Стиль — сверён с [DESIGN.md](./DESIGN.md).
- Коммит гранулярный, сухой `type(scope): что` (см. `cremniy-commit-style`).

---

## Куда это растёт

Сегодня реестр статический — пак вкомпилен. Следующий шаг видения — манифест пака
+ загрузка из `.cremniy`/директории паков, чтобы git/docker/поиск, а в перспективе
**STM32-паки взамен тулчейна ST**, ставились как расширения. Текущий шов
(`VIEWS` / `CENTER_PANELS` / `#[tauri::command]` + `bridge`) — это та граница, за
которую динамический загрузчик встанет, не ломая уже написанные паки.
