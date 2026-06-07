# Cremniy Design Canon

Единственный источник правды по визуальному стилю Cremniy. Когда нужно покрасить новый элемент или подобрать состояние — сверяйся отсюда, а не выдумывай палитру на лету.

## Принципы

1. **Нейтральная тёмная база без акцентов.** Cremniy — низкоуровневый IDE. Палитра приглушённая, монохромная по фону, акцентные цвета используются только для семантики (ошибка/предупреждение/успех) и для редкой подсветки активного элемента — не для декорации.
2. **Состояния через прозрачность белого.** Hover / selected / focused — это не «другой цвет», а тот же фон + `rgba(255,255,255,N)`. Это даёт согласованный вид на любом уровне elevation и не требует подбирать пары цветов.
3. **Минимум линий.** Бордеры — тонкие, полупрозрачные (`rgba(255,255,255,0.06–0.14)`). Никаких сплошных серых разделителей на тёмном фоне.
4. **Скругления — не «корпоративные».** 4–10px. Жирные радиусы (>12px) только у больших overlay карточек.
5. **Motion — короткий и стандартный.** 120/180 мс с `cubic-bezier(0.4,0,0.2,1)`. Никаких 400+мс ease-out перепрыгиваний.
6. **Типографика двухслойная.** UI — sans (Inter/Segoe UI Variable). Код / HEX / disasm / paths в Recent — mono (JetBrains Mono).

## Палитра

Источник истины: `frontend/src/shared/theme/tokens.css`. Ниже — что использовать когда.

### Surfaces (фоны)

| Token | Значение | Где |
|---|---|---|
| `--color-bg-base` | `#0e0f12` | Корневой фон окна, editor body |
| `--color-bg-panel` | `#13141a` | Sidebar, TitleBar, ToolPane, menu surface |
| `--color-bg-panel-alt` | `#191b22` | Чередующиеся строки (file tree alt rows), небольшие elevated блоки |
| `--color-bg-elevated` | `#1d1f27` | Popover, dropdown, Settings dialog |
| `--color-bg-overlay` | `rgba(20,22,28,0.92)` | Полупрозрачные оверлеи (Find dialog, Welcome backdrop) |

Правило: чем элемент «выше», тем светлее фон. base → panel → panel-alt → elevated. Никогда не используем фон темнее base.

### Текст

| Token | Значение | Где |
|---|---|---|
| `--color-text-primary` | `rgba(255,255,255,0.94)` | Основной текст: код, заголовки, имена файлов |
| `--color-text-secondary` | `rgba(255,255,255,0.66)` | Лейблы инпутов, подписи в карточках |
| `--color-text-tertiary` | `rgba(255,255,255,0.42)` | Пути в Recent, placeholder, секционные заголовки ("Recent projects") |
| `--color-text-on-accent` | `#ffffff` | Только на семантическом ярком фоне (toast error) |

Правило: вес текста контролируется **opacity**, а не другим тоном. Если тусклее — берёшь следующий уровень, не разводишь палитру.

### Бордеры

| Token | Значение | Где |
|---|---|---|
| `--color-border-muted` | `rgba(255,255,255,0.04)` | Едва заметные разделители (между rows в списке) |
| `--color-border-default` | `rgba(255,255,255,0.06)` | Карточки Welcome, тонкие линии в empty-state |
| `--color-border-pane` | `rgba(255,255,255,0.08)` | Pane-headers, основные перегородки |
| `--color-border-strong` | `rgba(255,255,255,0.14)` | Hover карточек, акцентируемые границы |
| `--color-border-menu` | `rgba(255,255,255,0.1)` | Popover / dropdown / dialog бордеры |

Правило: чем глубже фон, тем светлее бордер (чтобы оставался видимым).

### Семантика (используется редко, точечно)

| Token | Значение | Где |
|---|---|---|
| `--color-success` | `#21c55d` | Toast success, "Saved" badge |
| `--color-success-muted-bg` | `rgba(33,197,93,0.14)` | Подсветка успешной строки |
| `--color-success-border` | `rgba(33,197,93,0.5)` | Бордер success-карточек |
| `--color-warning` | `#d7ba7d` | Toast warning, нерешённое валидационное состояние |
| `--color-warning-muted-bg` | `rgba(215,186,125,0.14)` | Фон warning блока |
| `--color-warning-border` | `rgba(215,186,125,0.5)` | Бордер warning |
| `--color-error` | `#ef4444` | Toast error, deleted lines, Close-button hover |
| `--color-error-surface` | `rgba(239,68,68,0.14)` | Фон error блока |
| `--color-error-border` | `rgba(239,68,68,0.5)` | Бордер error |

Правило: семантические цвета только для **смысла** (ошибка, успех, предупреждение). Никогда не для «выделить кнопку» или «акцентировать важное».

### Accent (синий)

| Token | Значение | Где |
|---|---|---|
| `--color-accent` | `#2626d5` | Резервируется под будущие brand-моменты. Сейчас НЕ используется в UI. |
| `--color-accent-soft` | `rgba(86,156,214,0.18)` | Только селекция текста в editor, найденный match в Find |
| `--color-focus-ring` | `rgba(86,156,214,0.55)` | `:focus-visible` outline по умолчанию — единственное место где синий допустим на ui-элементах |

Правило: **не использовать синий для primary-кнопок, hover, selected**. Если хочется «выделить» — берём белый opacity. Синий = только ring и текстовая селекция в editor.

## Состояния

| Состояние | Реализация |
|---|---|
| **Default** | tokenизированный surface, без тени, бордер `--color-border-pane` |
| **Hover** | фон + `rgba(255,255,255,0.04–0.05)`, бордер на один шаг сильнее |
| **Active / pressed** | фон + `rgba(255,255,255,0.08)`, без перепрыгивания вниз |
| **Selected** | фон + `rgba(255,255,255,0.06)` (в menu) или `--color-accent-soft` (в editor) |
| **Focused (visible)** | `outline: 2px solid var(--color-focus-ring); outline-offset: 1px` |
| **Disabled** | `opacity: 0.4`, `cursor: not-allowed`, без изменения фона |
| **Error** | бордер `--color-error-border`, фон `--color-error-surface`, текст ошибки `--color-error` |
| **Success** | бордер `--color-success-border`, фон `--color-success-muted-bg` |

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

| Token | Значение | Где |
|---|---|---|
| `--radius-xs` | 3 | Иконки в rail, маленькие кнопки control |
| `--radius-sm` | 4 | Inputs, плоские list items |
| `--radius-md` | 6 | Стандартные кнопки, dropdown menu |
| `--radius-lg` | 10 | Карточки Welcome, dialog |
| `--radius-xl` | 14 | Большие overlay карточки (редко) |
| `--radius-pill` | 999 | Badges (если будут) |

## Тени

| Token | Где |
|---|---|
| `--shadow-sm` | Кнопки primary, поднятые элементы |
| `--shadow-md` | Карточки на hover |
| `--shadow-popover` | Dropdown, context menu, Select popup |
| `--shadow-overlay` | Modal dialog, Settings |

## Motion

| Token | Где |
|---|---|
| `--transition-fast` | 120ms — hover-tint, focus, мелкие иконки |
| `--transition-base` | 180ms — кнопки, инпуты, popover open |
| `--transition-slow` | 260ms — dialog enter/exit, slide-in toast |

Кривая по умолчанию — `--easing-standard: cubic-bezier(0.4,0,0.2,1)`. Никаких bounce / overshoot.

## Типографика

| Token | Стек | Где |
|---|---|---|
| `--font-family-sans` | Inter, Segoe UI Variable, Segoe UI, system-ui | Меню, кнопки, заголовки, label, recent имена |
| `--font-family-mono` | JetBrains Mono, Cascadia Code, Consolas | Код в editor, HEX viewer, disasm, terminal, paths в Recent |

| Размер | Где |
|---|---|
| 11–12px | Подписи под крупными элементами, статус-бар |
| 13px (`--font-size-base`) | Основной UI текст |
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
focus → border: rgba(255,255,255,0.22) + focus-ring
placeholder → opacity 0.4 (через --color-text-tertiary)
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
background: #1d1f27 (= elevated)
border: 1px solid rgba(255,255,255,0.1)
border-radius: 5
shadow: --shadow-popover
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
Имя слева — sans, size 0.82rem. Путь / sublabel справа — mono, size 0.72rem, `--color-text-tertiary`.

### Toast (NotificationOverlay)

См. `boundary/notifications/NotificationContext.tsx`. Левый бордер — семантический (success/warn/error border-токен). Фон — `--color-bg-elevated` + лёгкий tint цвета уровня. Auto-dismiss 3–8 сек по уровню.

### Dialog

```
background: --color-bg-elevated
border: 1px solid --color-border-pane
border-radius: --radius-lg
shadow: --shadow-overlay
padding: 1rem 1.25rem
backdrop: rgba(0,0,0,0.4) + blur(4px)
enter animation: cremniyFadeIn 0.18s
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

> Нужен «акцент» который не семантика → нет такого понятия. Если действительно надо — белый opacity primary-кнопка (`actionBtnPrimary` паттерн).

> Нужен focus ring → ничего не делать, есть глобальный `:focus-visible` в `index.css`.

## Связанные файлы

- Токены: `frontend/src/shared/theme/tokens.css`
- Глобалки (scroll, focus, body): `frontend/src/index.css`
- Notification (toast): `boundary/notifications/NotificationContext.tsx`
- Custom Select: `boundary/common/Select.tsx`
- TitleBar: `boundary/chrome/TitleBar.tsx`
- Welcome (canonical card layout): `boundary/welcome/WelcomeView.tsx`
