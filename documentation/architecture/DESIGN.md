# Cremniy Design Canon

Единственный источник правды по визуальному стилю Cremniy. Когда нужно покрасить новый элемент или подобрать состояние — сверяйся отсюда, а не выдумывай палитру на лету.

## Принципы

1. **Нейтральная тёмная база без акцентов.** Cremniy — низкоуровневый IDE. Палитра приглушённая, монохромная по фону, акцентные цвета используются только для семантики (ошибка/предупреждение/успех) и для редкой подсветки активного элемента — не для декорации.
2. **Состояния через прозрачность белого.** Hover / selected / focused — это не «другой цвет», а тот же фон + `rgba(255,255,255,N)`. Это даёт согласованный вид на любом уровне elevation и не требует подбирать пары цветов.
3. **Минимум линий.** Бордеры — тонкие, полупрозрачные (`rgba(255,255,255,0.06–0.14)`). Никаких сплошных серых разделителей на тёмном фоне.
4. **Скругления — не «корпоративные».** 4–10px. Жирные радиусы (>12px) только у больших overlay карточек.
5. **Motion — короткий и стандартный.** 120/180 мс с `cubic-bezier(0.4,0,0.2,1)`. Никаких 400+мс ease-out перепрыгиваний.
6. **Типографика моно-only.** Весь UI — моноширинный шрифт. `index.css` ставит `body { font-family: var(--font-family-mono) }`, отдельного sans-стека нет. Меню, кнопки, заголовки, код, HEX, disasm, paths в Recent — всё на JetBrains Mono.

## Палитра

Источник истины: `frontend/src/shared/theme/tokens.css`. Ниже — что использовать когда.

### Surfaces (фоны)

| Token | Значение | Где |
|---|---|---|
| `--color-bg-base` | `#262626` | Корневой фон окна (body) |
| `--color-bg-panel` | `#1f1f1f` | Sidebar, TitleBar, ToolPane, menu surface |
| `--color-bg-panel-alt` | `#2b2b2b` | Чередующиеся строки (file tree alt rows), небольшие elevated блоки |
| `--color-bg-editor` | `#1e1e1e` | Фон редактора кода (под Monaco vs-dark), breadcrumb / status chrome |
| `--color-list-alternate` | `#0a0f18` | Альтернативная строка в плотных списках |

Правило: чем элемент «выше», тем светлее фон. base → panel-alt. Никогда не используем фон темнее base.

### Текст

| Token | Значение | Где |
|---|---|---|
| `--color-text-primary` | `#ffffff` | Основной текст: код, заголовки, имена файлов |
| `--color-text-on-accent` | `#ffffff` | Текст поверх семантического / CTA фона |

Правило: приглушённый текст делается **opacity** на самом элементе, а не отдельным токеном тона — в `tokens.css` только эти два цвета текста. Токенов `--color-text-secondary` / `--color-text-tertiary` нет; компоненты, которым нужен тусклый текст, задают `opacity` локально (или используют `var(--color-text-tertiary, …)` с хардкодным фолбэком).

### Бордеры

| Token | Значение | Где |
|---|---|---|
| `--color-border-default` | `#1f1f1f` | Тонкие линии, разделители, бордеры по умолчанию |
| `--color-border-pane` | `#262626` | Pane-headers, основные перегородки |
| `--color-border-muted` | `#333333` | Чуть более заметная граница (читается на тёмном фоне) |
| `--color-border-menu` | `#111111` | Popover / dropdown / dialog / menu бордеры (тёмный кант) |
| `--color-border-accent` | `rgba(255,255,255,0.28)` | Акцентируемая граница (hover/active карточек) |

Бордеры — непрозрачный hex (`--color-border-*`), сливающийся с фоном; акцентируемая граница — `--color-border-accent` через белый opacity.

### Семантика (используется редко, точечно)

| Token | Значение | Где |
|---|---|---|
| `--color-success` | `#2c7c32` | Toast success, "Saved" badge |
| `--color-success-muted-bg` | `#163318` | Подсветка успешной строки / фон success блока |
| `--color-error` | `#bf3131` | Toast error, deleted lines, Close-button hover |
| `--color-error-surface` | `#4a2020` | Фон error блока |
| `--color-error-border` | `#ff5555` | Бордер error |

Правило: семантические цвета только для **смысла** (ошибка, успех). Warning-токенов в `tokens.css` нет. Никогда не для «выделить кнопку» или «акцентировать важное».

### Accent / selection (нейтральный)

«Accent» — это **не** бренд-цвет, а чуть более яркий нейтрал. Состояния (hover / selected / акцент) делаются прозрачностью белого.

| Token | Значение | Где |
|---|---|---|
| `--color-accent` | `rgba(255,255,255,0.55)` | Нейтральный «акцент» (яркий нейтрал); сюда же `html { accent-color }` для native-контролов |
| `--color-selection` | `rgba(255,255,255,0.18)` | Текстовая селекция (`::selection`), `<mark>` |
| `--color-hover-surface` | `rgba(255,255,255,0.06)` | Hover-фон поверхности |
| `--color-hover-tint` | `rgba(255,255,255,0.04)` | Лёгкий hover-tint |

Правило: **синий для primary-кнопок, hover, selected запрещён**. Хочешь «выделить» — белый opacity.

### CTA (единственный санкционированный цветной акцент)

Один цветной акцент на весь UI — светло-синий CTA, зарезервирован под самые важные действия (Commit, Initialize…). Везде остальное монохром; цвет тратится скупо, чтобы эти действия выделялись (см. комментарий в `tokens.css`).

| Token | Значение | Где |
|---|---|---|
| `--color-cta` | `#8ab4e8` | Фон primary-CTA (Commit / Initialize) |
| `--color-cta-hover` | `#9dc6f4` | Hover CTA |
| `--color-cta-text` | `#0e1d2d` | Текст на CTA |

### Git decorations (статус в Explorer)

Семантические цвета git-статуса (палитра VS Code) — второй санкционированный цвет вне CTA, только для смысла.

| Token | Значение | Где |
|---|---|---|
| `--git-untracked` | `#73c991` | Untracked файлы |
| `--git-added` | `#81b88b` | Added (staged) |
| `--git-modified` | `#e2c08d` | Modified |
| `--git-deleted` | `#c74e39` | Deleted |
| `--git-conflict` | `#e4676b` | Conflict |

### Menu bar / Scrollbars

| Token | Значение | Где |
|---|---|---|
| `--color-menubar-item-selected` | `#444444` | Выбранный пункт меню-бара |
| `--color-menubar-item-hover` | `#262626` | Hover пункта меню-бара |
| `--color-scrollbar-track` | `#1f1f1f` | Трек скроллбара |
| `--color-scrollbar-thumb` | `#262626` | Ползунок скроллбара |
| `--size-scrollbar` | `16px` | Толщина скроллбара |

## Состояния

| Состояние | Реализация |
|---|---|
| **Default** | tokenизированный surface, без тени, бордер `--color-border-pane` |
| **Hover** | фон + `rgba(255,255,255,0.04–0.05)`, бордер на один шаг сильнее |
| **Active / pressed** | фон + `rgba(255,255,255,0.08)`, без перепрыгивания вниз |
| **Selected** | фон + `rgba(255,255,255,0.06)` (= `--color-hover-surface` / `--color-selection` для текста) |
| **Focused (visible)** | задаётся точечно в компоненте (`:focus-visible` → `outline` белым opacity); глобального focus-ring нет |
| **Disabled** | `opacity: 0.4`, `cursor: not-allowed`, без изменения фона |
| **Error** | бордер `--color-error-border`, фон `--color-error-surface`, текст ошибки `--color-error` |
| **Success** | фон `--color-success-muted-bg`, текст / акцент `--color-success` |

## Spacing

Используем кратные `0.25rem` (4px), типичные значения:

| Token | rem | px | Где |
|---|---|---|---|
| `xs` | 0.25 | 4 | Внутри кнопок, gap иконка-текст |
| `sm` | 0.5 | 8 | Стандартный gap, padding inputs |
| `md` | 0.75 | 12 | Padding карточек, gap между секциями |
| `lg` | 1 | 16 | Между блоками формы |
| `xl` | 1.5–2 | 24–32 | Между большими секциями Welcome |

## Радиусы

> Токенов `--radius-*` в `tokens.css` нет — значения задаются числом прямо в компоненте. Ниже — соглашение по шкале.

| Значение | Где |
|---|---|
| 3 | Иконки в rail, маленькие кнопки control |
| 4 | Inputs, плоские list items |
| 6 | Стандартные кнопки, dropdown menu |
| 10 | Карточки Welcome, dialog |
| 14 | Большие overlay карточки (редко) |
| 999 | Badges (если будут) |

## Тени

> Токенов `--shadow-*` в `tokens.css` нет — `box-shadow` задаётся в компоненте. Соглашение: subtle тень только на elevated-элементах (popover, dropdown, Select popup, modal dialog / Settings), большой offset / мягкие brand-тени запрещены.

## Motion

> Токенов `--transition-*` / `--easing-standard` в `tokens.css` нет — длительность и кривая пишутся в компоненте. Соглашение по шкале:

| Длительность | Где |
|---|---|
| 120ms | hover-tint, focus, мелкие иконки |
| 180ms | кнопки, инпуты, popover open |
| 260ms | dialog enter/exit, slide-in toast |

Кривая по умолчанию — `cubic-bezier(0.4,0,0.2,1)`. Никаких bounce / overshoot. Единственные keyframes в `index.css` — `cremniyNotifSlideIn` (slide-in toast) и `disasmProgress` (индикатор прогресса дизассемблера).

## Типографика

UI **моно-only**: `index.css` ставит `body { font-family: var(--font-family-mono) }`, отдельного `--font-family-sans` нет. Весь интерфейс (меню, кнопки, заголовки, label, имена в Recent) и весь код/HEX/disasm/terminal — на одном моношрифте.

| Token | Стек | Где |
|---|---|---|
| `--font-family-mono` | `'JetBrains Mono', 'Consolas', monospace` | Весь UI и весь код |
| `--font-weight-emphasis` | `700` | Жирное выделение (акцент в тексте) |

| Размер | Где |
|---|---|
| 11px | Минимум; подписи, статус-бар |
| 12px (`--font-size-base`) | Основной UI текст |
| 14px | Заголовки секций |
| 16–18px | Hero / большие заголовки |

`line-height: 1.5` для текста, `1.35` для плотных таблиц/листов.

## Паттерны компонентов

### Button (default)

```
background: rgba(255,255,255,0.03)
border: 1px solid rgba(255,255,255,0.1)
border-radius: 4
padding: 0.4rem 0.8rem
font-size: 0.85rem
hover → background: rgba(255,255,255,0.07)
disabled → opacity: 0.4
```

### Button (primary)

Primary = «слегка ярче» обычной, **не цветная**:
```
background: rgba(255,255,255,0.08)
border: 1px solid rgba(255,255,255,0.18)
hover → background: rgba(255,255,255,0.12); border: rgba(255,255,255,0.26)
```

### Input

```
background: rgba(255,255,255,0.03)
border: 1px solid rgba(255,255,255,0.1)
border-radius: 4
padding: 0.4rem 0.6rem
focus → border: rgba(255,255,255,0.22) (focus-ring задаётся в компоненте, не глобально)
placeholder → opacity 0.4 (локально; токена --color-text-tertiary нет)
```

### Card (Welcome action card)

```
background: rgba(255,255,255,0.018)
border: 1px solid rgba(255,255,255,0.06)
border-radius: 8
padding: 0.85rem 0.9rem 0.8rem
align-items: flex-start  (контент слева сверху, как в Cursor)
gap: 0.55rem (icon → label)
hover → background: rgba(255,255,255,0.04); border: rgba(255,255,255,0.12)
```

### Select (наш, не native)

См. `boundary/common/Select.tsx`. Trigger = Input. Popup:
```
background: --color-bg-panel-alt (#2b2b2b)
border: 1px solid --color-border-menu (#111111)
border-radius: 5
shadow: subtle box-shadow (задаётся в компоненте, токена нет)
option default: padding 0.32rem 0.7rem
option hover: rgba(255,255,255,0.06)
option selected: rgba(255,255,255,0.04) + текст primary (НЕ цветной)
option selected+hover: rgba(255,255,255,0.1)
```

### Context menu / dropdown

То же что Select-popup. Item:
- padding `0.3rem 0.65rem`
- font-size 13px
- hover `rgba(255,255,255,0.05)`
- destructive items (Clear / Delete) — текст `--color-error`, hover фон `rgba(239,68,68,0.08)`

### List item (Recent row, FileTree row)

```
display: flex; align-items: baseline; gap: 1rem
padding: 0.35–0.45rem 0.4–0.6rem
border-radius: 4
hover → background: rgba(255,255,255,0.035)
selected → background: rgba(255,255,255,0.06) (НЕ синий)
```
Имя слева, size 0.82rem. Путь / sublabel справа — size 0.72rem, приглушённый opacity (всё mono, см. «Типографика»).

### Toast (NotificationOverlay)

См. `boundary/notifications/NotificationContext.tsx`. Левый бордер — семантический (success/error border-токен; warning-токена нет). Фон — `--color-bg-panel-alt` + лёгкий tint цвета уровня. Анимация входа — `cremniyNotifSlideIn`. Auto-dismiss 3–8 сек по уровню.

### Dialog

```
background: --color-bg-panel-alt
border: 1px solid --color-border-pane
border-radius: 10 (число; токена --radius-lg нет)
shadow: subtle box-shadow (в компоненте; токена нет)
padding: 1rem 1.25rem
backdrop: rgba(0,0,0,0.4) + blur(4px)
enter animation: задаётся в компоненте (keyframes cremniyFadeIn не существует)
```

### Center panel (вкладка Рабочего поля)

Большой экран, открытый вкладкой в центре (Settings, Advanced Git). Стандарт плотности —
**компактный**, под chrome (12px-моно), чтобы во всю ширину поля не разъезжалось:

```
двухпанельно: nav-rail слева ~9.5rem + контент справа
шрифты: заголовок секции ~0.92rem, строки/контролы 0.78–0.8rem, подписи 0.66–0.72rem
паддинги: строки секций ~0.28–0.5rem, межсекционные отступы ~1rem
КЛЮЧЕВОЕ: контент .mainScroll { width:100%; max-width: 44rem } — колонка, не тянется
embedded-режим (в табе): без backdrop/Esc, width/height 100%
```

Эталон — `AdvancedGitDialog` / `SettingsDialog` (общие имена классов, одинаковые значения).
Новая центр-панель держит ту же плотность (см. PLUGINS.md → «Добавить center-пак»).

## Иконки

- Всегда **SVG inline**, `viewBox="0 0 24 24"`, `stroke="currentColor" fill="none" stroke-width="1.5"`. Размер задаётся снаружи через `width/height` (16–22px).
- Никаких icon-fonts, никаких emojis в UI (emoji можно в toast как декорация, но не как UI-action).
- Если нужна новая иконка — стиль Lucide/Phosphor outline. Тонкий stroke, прозрачное заполнение.

## TitleBar (window chrome)

- Высота 36px, фон `--color-bg-panel`, бордер снизу `rgba(255,255,255,0.06)`.
- Logo grayscale 0.7, размер 16px.
- Brand text `font-family: mono`, size 12px, letter-spacing 0.02em, opacity 0.85.
- Control buttons 46px ширина, прозрачный фон, hover `rgba(255,255,255,0.07)`. Close-hover = `#e81123` (Windows-стандарт).

## Что НЕ делать

- ❌ Синий/фиолетовый/розовый primary-кнопки, баджи, фоны
- ❌ Градиенты (никогда — Cremniy не маркетинговый сайт)
- ❌ Border-radius >14px (нет «леденцов»)
- ❌ Жирные/мягкие тени с большим offset — только subtle box-shadow на elevated элементах
- ❌ Анимации > 300мс
- ❌ Контрастные «брендовые» полосы наверху диалогов
- ❌ Emoji в UI-actions (только в toast/notifications как декорация)
- ❌ `outline: 0` без замены через `:focus-visible`
- ❌ Текст < 11px

## Шпаргалка для типичных задач

> Нужно подсветить «активный» пункт списка → `rgba(255,255,255,0.06)`, текст primary.

> Нужно показать «выполнено успешно» → toast success, не подкрашиваем сам элемент.

> Нужно показать ошибку валидации в форме → бордер инпута `--color-error-border`, текст ошибки под полем `--color-error` 0.85rem.

> Нужен hover на карточке → фон с +`rgba(255,255,255,0.02)` от текущего + бордер на один шаг сильнее.

> Нужен «акцент» который не семантика → белый opacity primary-кнопка (`actionBtnPrimary` паттерн). Цветной акцент только для главного CTA (Commit/Initialize) — `--color-cta`.

> Нужен focus ring → задать `:focus-visible` outline (белый opacity) в самом компоненте. Глобального focus-ring в `index.css` нет — там только `html { accent-color: rgba(255,255,255,0.55) }`, а компоненты по умолчанию ставят `outline: none`.

## Связанные файлы

- Токены: `frontend/src/shared/theme/tokens.css`
- Глобалки (scroll, selection, body, system-color overrides): `frontend/src/index.css`
- Notification (toast): `boundary/notifications/NotificationContext.tsx`
- Custom Select: `boundary/common/Select.tsx`
- TitleBar: `boundary/chrome/TitleBar.tsx`
- Welcome (canonical card layout): `boundary/welcome/WelcomeView.tsx`
