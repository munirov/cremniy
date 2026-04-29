<div align="center">

<img src="docs/cremniy_icon_stroke.svg" width="250" alt="Cremniy logo">

<br>
<h3>Cremniy</h3>
<h6>Все инструменты для низкоуровневой разработки объединены и связаны в одном приложении — пишите код, редактируйте байты и анализируйте бинарники без лишних окон</h6>

[![License](https://img.shields.io/github/license/igmunv/cremniy?color=orange&style=flat-square)](LICENSE)
[![Contributions Welcome](https://img.shields.io/badge/Contributions-Welcome-brightgreen?style=flat-square)](CONTRIBUTING.md)
[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)
<br>
[![Tauri](https://img.shields.io/badge/Tauri-2-24C8D8?style=flat-square&logo=tauri)](https://tauri.app/)
[![React](https://img.shields.io/badge/React-18-61DAFB?style=flat-square&logo=react)](https://react.dev/)

[English](README.md) • Русский

</div>

<br>

## Десктопное приложение (Tauri + React)

В репозитории официальная десктопная сборка — **Tauri 2 + React + TypeScript** в каталоге [`frontend/`](frontend/). См. [ADR-001](ai_docs/develop/architecture/ADR-001-tauri-desktop-primary.md) и [ADR-002](ai_docs/develop/architecture/ADR-002-qt-sources-removed.md).

**Разработка:**

```bash
cd frontend
npm install
npm run dev
npm run tauri:dev
```

**Продакшен-сборка локально:**

```bash
cd frontend
npm run tauri:build
```

Установщики и бандлы — в `frontend/src-tauri/target/release/bundle/`.

Историческое приложение на **Qt/C++** из каталога **`src/` удалено** из этого репозитория. Последняя ревизия с деревом `src/` помечена тегом **`pre-qt-removal-2026-05-01`** (`git checkout` по тегу — если нужны исходники для справки).

> **Важно:** Qt отвечал не только за «внешний вид», а за весь старый стек десктопа (виджеты, окна, интеграция инструментов). Текущая кодовая база переносит функциональность в веб-UI и Rust-оболочку Tauri.

---

## Что такое Cremniy?

**Cremniy** — интегрированная среда для низкоуровневой разработки. Вместо того чтобы держать HEX-редактор в одном окне, дизассемблер в другом, а редактор кода в третьем — всё это объединено и связано в одном удобном приложении.

**Ориентирован на:**

- 🛠 Разработчиков системного ПО
- 🔍 Reverse-инженеров
- 🔐 Специалистов по информационной безопасности
- 📡 Разработчиков embedded-систем

## Почему Cremniy?

Низкоуровневая разработка сегодня — это редактор кода, HEX-редактор, дизассемблер, отладчик, открытые **в разных окнах**.

Вы постоянно **переключаетесь** между разными окнами, и при этом инструменты **не связаны** между собой.

#### **Cremniy решает это!**
- 🔘 Всё находится в одном месте
- 🔗 Всё связано между собой
- 💻 Единый workflow

![out](https://github.com/user-attachments/assets/f5e9c520-fb31-45cc-ab11-17eff66d7069)

## Возможности

### Доступно сейчас

Ранние сборки **Tauri** закрывают оболочку, сценарий воркспейса и визуальный паритет стартового/IDE-хрома. Полный паритет инструментов (HEX, дизассемблер, терминал и т.д.) — в [ROADMAP.md](ROADMAP_ru.md) и `ai_docs/`.

### В планах

- Полноценный **HEX-редактор** и просмотр бинарных форматов в UI
- UI **дизассемблера** поверх Rust-контракта subprocess
- **Отладчик** — пошаговое выполнение, просмотр регистров и памяти
- **Визуализация памяти** — наглядные карты расположения и выделения памяти

## Участие в разработке

Вклад в проект **приветствуется**.

Будь то исправление ошибок, новая функциональность или улучшение документации — открывайте issue или отправляйте pull request.

Все участники указываются в [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) и упоминаются в видео на [YouTube-канале](https://www.youtube.com/@igmunv).

Подробнее — в [CONTRIBUTING.md](CONTRIBUTING_ru.md).

## Лицензия

Распространяется на условиях, описанных в [LICENSE](LICENSE).
