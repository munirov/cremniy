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


## Coding Style Guidelines

This is a very important part of each project, please use it primarily
in current project scope. 
Guidelines represented here bases mostly on Qt framework coding rules.
That's why the most part of Cremniy bases on Qt 6.

Next following regions fully describes coding style rules.

### Comments declaration
Instead of single-line comments (a.k.a. `//`) we use block-comments (`/**/`)!

```cpp
/* Window setup */
this->setWindowState(Qt::WindowMaximized);
this->setWindowTitle("Cremniy"); /* <-- correct */

this->setWindowTitle("") // incorrect
```
Block-comments format usually could been set up in the code editor.
For anyway, make sure that big multiline comment (a.k.a. `/**/`) consists of
direct stars column and each star what follows next are having same column position.

We use Doxygen comments. Here is the good source to understand this feature
 - [Documenting C++ Code — LSST DM Developer Guide main...](https://developer.lsst.io/cpp/api-docs.html)  

```cpp
/**
 * @brief This is a correct doc comment! 
 * @param projectPath Make sure, your stars follows one by one (and sleep well)
 */
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)
```

After this example, represented examples below are **WRONG**.

```cpp
/**
* @brief This is an incorrect doc comment!
* @param projectPath Stars are has bad position 
*/
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)

/**
 @brief This is an incorrect doc too!! 
 @param projectPath No stars? ;-; 
 */
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)

/**
 * \brief This is an incorrect doc!!! 
 * \param projectPath Please, use "@" tags instead of "\". And configure your workspace the same way 
 */
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)

/**
 * @brief 这份文件很好，但不是用英文写的。 
 * Unfortunately, we don't understand what you want. 
 * Please write comments in English!
 * @param projectPath Remember it
 */
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)
```

### Declare `Classes`/`Structures` right

We accepted that:
 - All objects which have body `{...}` (e.g. functions, conditions, counters) are K&R styled;
 - All variables, pointers, fields will be `camelCase`;
 - Classes, structures, enum-s will be `PascalCase`.   

Hungarian notation doesn't use there. But once thing you may to know about.
All class fields are have `m_` prefix, what means "member_" prefix.

```cpp
/* 
 * PascalCase for classes/structs/enums/unions 
 */
class IDEWindow : public QMainWindow { 
private:
    /* 
     * camelCase for others
     */
    QMenuBar * m_menuBar;
    /*
     * SCREAMING_SNAKE for program constants
     */
    const qint64 WINDOW_WIDTH = 900;
    /*
     * Local variables definition  
     */
    static void setTerminalWidget() {
        /* Usually, declare it through the "auto" */
        auto path = model->filePath(index); /* <-- correct. Because filePath(index) returns QString */
        QString fileName = model->fileName(index); /* <-- not correct! */
        
        /* But also, _use explicit declaration_ when it necessary */
        QMenu menu();
    }
}
```

### Declare References & Pointers right!

This part very important too! We decided to declare references and pointers
with the special character after the type (at the right-side).

```cpp
/* correct definition | correct cast style */
auto* model = dynamic_cast<QFileSystemModel*>(m_filesTreeView->model()); 
/* incorrect: what is that? | incorrect space in <T *> */
auto * model = dynamic_cast<QFileSystemModel *>(m_filesTreeView->model()); 
/* incorrect: this is not dereference */
auto  *model = dynamic_cast< QFileSystemModel * >(m_filesTreeView->model());

/*bad! we're expecting pointer/reference readability */
auto model = dynamic_cast<QFileSystemModel *>(m_filesTreeView->model());
```

This rule still uses in bit-fields declaration

```cpp
/* correct definition | correct dereference*/
auto& model = *modelPointer; 
/* explicit declaration redundant | bad dereference spacing */
QFileSystemModel& modelRef = * modelPointer; 

#pragma push(pack(1))
struct UInt48 {
    /* correct 48-bit field */
    uint64_t lBytes: 48; 
    uint64_t hBytes: 16;
} /*sizeof(UInt48) = 8*/
#pragma pop()
```

Last rule, accepted in this project scope is functions declatation.
Functions, what have input arguments which length more than 80 symbols, 
are will be wrapped at the new line; And closing brace will be wrapped at the next line after last argument.

```cpp
/*correct Qt macro call*/
connect(
    this,
    &IDEWindow::saveFileSignal,
    m_filesTabWidget,
    &FilesTabWidget::saveFileSlot
);
/*incorrect. 2 tabs size. Closing brace not at the new line.*/
connect(
        this,
        &IDEWindow::saveFileSignal,
        m_filesTabWidget,
        &FilesTabWidget::saveFileSlot);

connect(
    this,
    &IDEWindow::saveFileSignal,
    m_filesTabWidget,
    &FilesTabWidget::saveFileSlot
    ); /* <-- incorrect. Bad closing brace placement */
```


## Pull Requests

### Requirements

- A pull request should address **one specific task** or a tightly related group of tasks.
- Do not combine **different changes** in a single PR (e.g., new features, refactoring, and fixes at the same time).
- Large changes should be **split into multiple** separate PRs.
- Link your PR to a task if one exists ([see details below](CONTRIBUTING.md#linking-pr-to-tasks))

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
