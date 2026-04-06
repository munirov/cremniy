<div align="center">

[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)

English • [Русский](CONTRIBUTING_ru.md)
	
</div>

# Contribution

Thank you for your interest in the Cremniy project.  
Any help in improving the project is highly appreciated.

## Ways to Contribute

You can help in several ways:

- Report bugs (create a new **Issue** using the `Bug report` template)
- Suggest new features (create a new **Issue** with the `idea` tag)
- Improve documentation
- Submit pull requests ([more info](CONTRIBUTING.md#pull-requests))

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
- Link your PR to a task if one exists ([see details below](CONTRIBUTING.md#linking-pr-to-tasks))

### Submission

1. Fork the repository
2. Create a new branch
3. Make your changes
4. Sync your branch with the main `main` branch and resolve conflicts if any
5. Create a pull request with a clear description

### Linking PR to tasks

Each Pull Request should **clearly indicate which task or Issue it addresses**, if such a [task](ROADMAP.md) or Issue exists.  
If there is no corresponding task, simply describe the changes in the PR.

## Acknowledgements

All contributors will be added to [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)  
and mentioned at the end of each video on the [YouTube channel](https://www.youtube.com/@igmunv)
