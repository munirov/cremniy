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
```

The format of multi-line comments is usually fixed by the code editor. Nevertheless,  
subsequent lines must follow exactly behind the previous one, and be accompanied by an asterisk at the beginning of each comment line.

We use Doxygen, and we recommend it to you. Here is a decent source explaining what it is:
 - [Documenting C++ Code — LSST DM Developer Guide main...](https://developer.lsst.io/cpp/api-docs.html)

```cpp
/**
 * @brief This is a correct doc comment 
 * @param projectPath Make sure, your stars follows one by one (and sleep well)
 */
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)
```

The listings below show how comments must **not** be formatted:

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
 * @brief Эта документация прекрасна, но она написана не на английском. Такое недопускается! 
 * @param projectPath Имейте это ввиду.
 */
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)
```

### 7.2. Class, Structure, and Other Unit Declarations

Please note that:
 - All objects that have a body (e.g., a function or a conditional statement) must be formatted in K&R style;
 - All variables, fields, and functions, regardless of their modifiers, must be written in `camelCase`;
 - Classes, structures, and enums are written in `PascalCase`.

Hungarian notation is not allowed, except for declarations of private fields inside a class/struct.

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

### 7.3. Pointers, References, and Large Functions

This is perhaps the strangest and most unfair decision to note.  
Pointers are always declared with the asterisk placed closer to the data type.

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

This rule also applies to references and bit-fields.

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

The last rule is about calling large functions.  
Functions whose arguments do not fit within 80 characters or that accept strictly more than 3 arguments are called as follows:

```cpp
/*correct Qt macro call*/
connect(
    this,
    &IDEWindow::saveFileSignal,
    m_filesTabWidget,
    &FilesTabWidget::saveFileSlot
);
```

The distance between the function name and its arguments is exactly one tab.

```cpp
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

## 8. Styling UI Text Elements

All user-visible text elements should be wrapped using standard Qt tools:

- `tr()` – for regular text elements that are not constants and `static`.
- `QT_TRANSLATE_NOOP(context, text)` – declaration for Qt to ensure the text is included in the translation file.
- `QCoreApplication::translate(context, text)` – for static constants, the translation will be applied immediately.

#### `tr()`

```c++
m_createFile = new QAction(tr("Create File"), this);
m_createDir  = new QAction(tr("Create Folder"), this);
m_delete     = new QAction(tr("Delete"), this);
m_rename     = new QAction(tr("Rename"), this);
m_open       = new QAction(tr("Open"), this);
```

#### `QT_TRANSLATE_NOOP(context, text)`

Used for static arrays and constants. It doesn't translate itself; it marks the text for `lupdate`. The translation is applied later via `QCoreApplication::translate()` or `tr()`.

> ⚠️ The context in `QT_TRANSLATE_NOOP` and `QCoreApplication::translate()` must match.

```c++
static const RefRow kRefRows[] = {
    {"Esc", "01", QT_TRANSLATE_NOOP("KeyboardScanCodesRef", "Break code: 81")},
    {"1",   "02", QT_TRANSLATE_NOOP("KeyboardScanCodesRef", "… 0 (top row) 0B")},
    // ...
};

for (int i = 0; i < n; ++i) {
    // ....
    m_table->setItem(i, 2, new QTableWidgetItem(
        QCoreApplication::translate("KeyboardScanCodesRef", kRefRows[i].notes)
    ));
}
```

If rendering occurs in the same class, you can use `tr()` directly:
> The key point is that the context you define for static elements has the same name as the class. In the `DataConverterDialog` example, according to the Qt standard, the context name is specified exactly this way.
```c++
static const UnitInfo kUnits[] = {
    { QT_TRANSLATE_NOOP("DataConverterDialog", "Bits"),      "Bit", 1.0 / 8.0 },
    { QT_TRANSLATE_NOOP("DataConverterDialog", "Bytes"),     "Byte", 1.0 },
    { QT_TRANSLATE_NOOP("DataConverterDialog", "Kilobytes"), "KB",   1024.0 },
    // ...
};

for (int i = 0; i < kUnitCount; ++i) {
    m_form->addRow(tr(kUnits[i].label), rowWidget);
}
```

#### `QMessageBox` with custom buttons

If you've installed your own buttons, wrap their text in `tr()`:

```c++
QMessageBox question_save_file(
    QMessageBox::Question,
    tr("Save File"),
    tr("Do you want to save this file?"),
    QMessageBox::NoButton,
    this
);

const auto yes    = question_save_file.addButton(tr("Yes"),    QMessageBox::YesRole);
const auto no     = question_save_file.addButton(tr("No"),     QMessageBox::NoRole);
const auto cancel = question_save_file.addButton(tr("Cancel"), QMessageBox::RejectRole);

question_save_file.exec();

const auto reply = question_save_file.clickedButton();
if (reply == yes)    tab->saveFile();
else if (reply == cancel) return;
```

#### Updating file translations

After finishing working with the code, run the `lupdate` utility:

```bash
lupdate src -ts src/resources/locale/translations/app_ru.ts
```

Then you need to process the file and fill in all fields with the "unfinished" status:

```xml
<context>
    <name>QHexView</name> <-- This is your context
    <message>
        <location filename="../../../libs/HexEditor/src/qhexview.cpp" line="378"/> <-- The place where you marked the expression as `tr()`
        <location filename="../../../libs/HexEditor/src/qhexview.cpp" line="397"/>
        <location filename="../../../libs/HexEditor/src/qhexview.cpp" line="405"/>
        <source>Go to</source> <-- text that was marked with tr()
        <translation type="unfinished">this text must be translated</translation>
    </message>
 </context>
```
