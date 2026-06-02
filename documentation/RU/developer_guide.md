# Руководство разработчика

Cremniy — Tauri + React. UI в WebView, нативная часть на Rust. Один процесс, один инсталлятор.

## Структура

```
source/
├── frontend/          # BMFP-фронт: Vite + React + TS
└── backend/           # Tauri-крейт на Rust: окно, IPC, доступ к ОС, запуск процессов
```

Слои фронта — [BMFP](../architecture/BMFP.md). Репозиторий целиком — [BMAP](../architecture/BMAP.md).

## Запуск / сборка

```
cd source/frontend
npm install
npm run tauri:dev      # dev
npm run tauri:build    # инсталлятор
```

`tauri:dev` — десктоп с hot-reload WebView. `tauri:build` — инсталлятор под платформу
(`.msi`, `.dmg`, `.AppImage`).

## Добавление фичи

Фича с данными, UI и IPC:

1. **DTO** в `domain/<feature>/` — схема и типы.
2. **Обёртка Tauri** в `infrastructure/tauri/<feature>.ts` — типизированные `invoke`.
3. **Сервис** в `domain/<feature>/<feature>.service.ts` — оркестрация обёрток и хранилища.
4. **UI** в `boundary/<feature>/` — компоненты читают через хуки, действия зовут сервис.
5. **Нативная команда** в `source/backend/src/<feature>.rs` — реализует системную работу;
   объявляется в `lib.rs`.
6. **Агентская команда** — регистрируется через `registerAgentCommands` по
   [AGENT_CONTROL](../architecture/AGENT_CONTROL.md).
7. **Тесты** рядом с кодом.

## Нативная оболочка

Rust-крейт в `source/backend/`. Каждая область — отдельный модуль: `process.rs` (хардненный
раннер), `disassembly.rs`, `terminal.rs`. `lib.rs` регистрирует Tauri-команды.

Добавляя нативную команду:

- валидируй входы на границе;
- бизнес-логику держи в чистой функции внутри модуля;
- сырые пути наружу не отдавай — разрешай и ограничивай в пределах workspace.

## Тесты

- Фронт: Vitest + Testing Library, рядом с кодом.
- Бэк: `cargo test`.
- Репо / E2E: `tests/` в корне (опционально).
