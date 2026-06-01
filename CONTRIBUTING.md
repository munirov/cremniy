<div align="center">

[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)

**Language / Язык:** use one section below (only one stays open in modern browsers).  
**English** — open the first block · **Русский** — откройте второй блок.

</div>

<details name="contributing-lang" open>
<summary><strong>English</strong></summary>

# Contribution

Thank you for your interest in the Cremniy project.  
Any help in improving the project is highly appreciated.

## Ways to Contribute

You can help in several ways:

- Report bugs (create a new **Issue** using the `Bug report` template)
- Suggest new features (create a new **Issue** with the `idea` tag)
- Improve documentation
- Submit pull requests ([more info](#pull-requests))

## Roadmap

All current **tasks** and **project plans** are gathered in the [roadmap](ROADMAP.md).  
Before creating an Issue or PR, it is **recommended to check** what has already been planned to **avoid duplicate work**.

## Language Policy

To keep the project accessible to international contributors, **all issues, pull requests, and commit messages must be written in English**. 

## Working with Branches

Only two branches are officially maintained in the main repository:

- **main**: the stable version of the project. Always contains production-ready code.
- **dev**: the active development branch. New features for the next release are created and tested here. Once development is complete, **dev** is merged into **main** to release a MINOR version.

All other branches (`feature/...`, `fix/...`) are created **in your fork** when working on a task or bug fix:

- **feature/...**: branches for new features (created from `dev`). After completion, a PR is created to merge into `dev`.
- **fix/...**: branches for bug fixes (created from `main`). After completion, a PR is created to merge into `main`. Once merged into `main`, the bugfix is also merged into `dev` to include the changes in the development version.

## Pull Requests

### Requirements

- A pull request should address **one specific task** or a tightly related group of tasks.
- Do not combine **different changes** in a single PR (e.g., new features, refactoring, and fixes at the same time).
- Large changes should be **split into multiple** separate PRs.
- Link your PR to a task if one exists ([see details below](#linking-pr-to-tasks))

### Submission

1. Fork the repository
2. Create a new branch from the appropriate base branch:
   - `dev` for new features
   - `main` for bug fixes
3. Make your changes
4. Sync your branch with the base branch (`dev` or `main`) and resolve any conflicts
5. Open a pull request to the appropriate branch (`dev` or `main`) with a clear description or a linked Issue

### Linking PR to tasks

Each Pull Request should **clearly indicate which task or Issue it addresses**, if such a [task](ROADMAP.md) or Issue exists.  
If there is no corresponding task, simply describe the changes in the PR.

## Acknowledgements

All contributors will be added to [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)  
and mentioned at the end of each video on the [YouTube channel](https://www.youtube.com/@igmunv)

</details>

<details name="contributing-lang">
<summary><strong>Русский</strong></summary>

# Контрибьюция

Спасибо за ваш интерес к проекту Cremniy.  
Любая помощь в улучшении проекта приветствуется.

## Способы контрибуции

Вы можете помочь несколькими способами:

- сообщать об ошибках (создайте новый **Issue** по шаблону `Bug report`)
- предлагать новые функции (создайте новый **Issue** с тегом `idea`)
- улучшать документацию
- отправлять pull request ([подробнее](#contributing-ru-pull-requests))

## План развития

Все текущие **задачи** и **планы** развития проекта **собраны** в [дорожной карте](ROADMAP.md).  
Перед созданием Issue или PR **рекомендуем посмотреть**, что уже запланировано, чтобы **не дублировать работу**.

## Языковая политика

Чтобы проект оставался доступным для международных участников, **все Issues, Pull Requests и сообщения коммитов должны быть написаны на английском языке.**

## Работа с ветками

В основном репозитории поддерживаются только две ветки:

- **main**: стабильная версия проекта. Всегда содержит готовый к использованию код.
- **dev**: ветка активной разработки. Здесь создаются и тестируются новые фичи для следующего релиза. После завершения разработки фичи, **dev** вливается в **main** — выпускается MINOR-релиз.

Все остальные ветки (`feature/...`, `fix/...`) создаются **в вашем форке**, когда вы работаете над задачей или багфиксом:

- **feature/...**: ветки для новых функций (создаются от `dev`). После завершения работы создаётся PR в `dev`.
- **fix/...**: ветки для исправления багов (создаются от `main`). После завершения работы создаётся PR в `main`. После мержа в `main` багфикс также мержится в `dev`, чтобы изменения попали в разрабатываемую версию.

<span id="contributing-ru-pull-requests"></span>

## Оформление Pull Request

### Требования

- Pull request должен решать **одну конкретную задачу** или группу тесно связанных задач
- Не объединяйте в одном PR **разные изменения** (например: новые функции, рефакторинг и фиксы одновременно)
- Крупные изменения **разбивайте на несколько** отдельных PR
- Свяжите PR с задачей, если таковая существует ([ниже подробнее](#contributing-ru-linking-pr))

### Отправка

1. Сделайте fork репозитория
2. Создайте новую ветку от соответствующей базовой ветки:
   - `dev` для новых функций (feature)
   - `main` для исправления багов (fix)
3. Внесите ваши изменения
4. Синхронизируйте вашу ветку с базовой веткой (`dev` или `main`) и решите конфликты, если они есть
5. Создайте pull request в соответствующую ветку (`dev` или `main`) с понятным описанием или прикреплением Issue

<span id="contributing-ru-linking-pr"></span>

### Связывание PR с задачами

Каждый Pull Request должен **явно указывать, какую задачу или Issue он решает**, если такая [задача](ROADMAP.md) или Issue существует.  
Если соответствующей задачи нет, просто опишите изменения в PR.

## Благодарность

Все контрибьюторы будут добавлены в [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)  
и упомянуты в конце каждого видео на [YouTube-канале](https://www.youtube.com/@igmunv)

</details>
