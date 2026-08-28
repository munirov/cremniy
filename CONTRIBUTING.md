<div align="center">

[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)

English • [Русский](CONTRIBUTING_ru.md)
	
</div>

# Contribution

Thank you for your interest in the Cremniy project.  
Any help in improving the project is highly appreciated.

## Table of Contents

- [1. Ways to Contribute](#1-ways-to-contribute)
- [2. Where to Find Tasks](#2-where-to-find-tasks)
- [3. Pull Requests](#3-pull-requests)
- [4. Acknowledgements](#4-acknowledgements)
- [5. Language Policy](#5-language-policy)
- [6. Working with Branches](#6-working-with-branches)
- [7. Coding Style Guidelines](#7-coding-style-guidelines)
	- [7.1. Comments](#71-comments)
 	- [7.2. Class, Structure, and Other Unit Declarations](#72-class-structure-and-other-unit-declarations)
	- [7.3. Pointers, References, and Large Functions](#73-pointers-references-and-large-functions)
- [8. Styling UI Text Elements](#8-styling-ui-text-elements)

## 1. Ways to Contribute

You can help in several ways:

- Report bugs (create a new **Issue** using the `Bug report` template)
- Suggest new features (create a new **Issue** using the `Feature` template)
- Improve documentation
- Submit pull requests ([more info](CONTRIBUTING.md#pull-requests))

## 2. Where to Find Tasks

All tasks can be found in [**GitHub Projects**](https://github.com/orgs/munirov/projects/2/views/1)

> [!WARNING]
> If you would like to take on a task, please leave a comment on the corresponding [Issue](https://github.com/munirov/cremniy/issues). This helps prevent duplicate work.
>
> Additionally, once you submit a Pull Request, reference the corresponding [Issue](https://github.com/munirov/cremniy/issues) in the PR description using `Closes #ISSUE_NUMBER`.

## 3. Pull Requests

Before submitting a PR, **please** read the requirements and instructions provided below.

### Requirements

- A pull request should address **one specific task** or a tightly related group of tasks.
- Do not combine **different changes** in a single PR (e.g., new features, refactoring, and fixes at the same time).
- Large changes should be **split into multiple** separate PRs.
- Link your PR to a task if one exists ([see details below](CONTRIBUTING.md#linking-pr-to-tasks)).

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

## 4. Acknowledgements

All contributors will be **permanently added to the "Acknowledgements" window** inside Cremniy itself  
and will be **mentioned at the end of each video** on the [YouTube channel](https://www.youtube.com/@igmunv).

## 5. Language Policy

To keep the project accessible to international contributors, **all issues, pull requests, commit messages, and code comments must be written in English**.

## 6. Working with Branches

Only two branches are officially maintained in the main repository:

- **main**: the stable version of the project. Always contains production-ready code.
- **dev**: the active development branch. New features for the next release are created and tested here. Once development is complete, **dev** is merged into **main** to release a MINOR version.

All other branches (`feature/...`, `fix/...`) are created **in your fork** when working on a task or bug fix:

- **feature/...**: branches for new features (created from `dev`). After completion, a PR is created to merge into `dev`.
- **fix/...**: branches for bug fixes (created from `main`). After completion, a PR is created to merge into `main`. Once merged into `main`, the bugfix is also merged into `dev` to include the changes in the development version.

## 7. Coding Style Guidelines

It is equally important to declare the style used in the project.  
The decisions presented in the sections below are based on the commonly accepted rules used in Qt,  
as a large portion of the project uses this framework. Differences in code style create  
confusion and hinder further analysis and development.

Project fragments will be used in the nested sections for clarity.

### 7.1. Comments

Instead of single-line comments (`//`), multi-line comments are used, as shown in the listing below.  
Note that comments must be written in English.

```cpp
/* Window setup */
this->setWindowState(Qt::WindowMaximized);
this->setWindowTitle("Cremniy"); /* <-- correct */

this->setWindowTitle("") // incorrect
