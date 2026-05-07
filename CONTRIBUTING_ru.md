<div align="center">

[![Community](https://img.shields.io/badge/Community-Telegram-blue?logo=telegram&style=flat-square)](https://t.me/cremniy_com)

[English](CONTRIBUTING.md) • Русский
	
</div>

# Контрибьюция

Спасибо за ваш интерес к проекту Cremniy.  
Любая помощь в улучшении проекта приветствуется.

## Способы контрибуции

Вы можете помочь несколькими способами:

- сообщать об ошибках (создайте новый **Issue** по шаблону `Bug report`)
- предлагать новые функции (создайте новый **Issue** с тегом `idea`)
- улучшать документацию
- отправлять pull request ([подробнее](CONTRIBUTING_ru.md#pull-requests))

## План развития

Все текущие **задачи** и **планы** развития проекта **собраны** в [дорожной карте](ROADMAP_ru.md).  
Перед созданием Issue или PR **рекомендуем посмотреть**, что уже запланировано, чтобы **не дублировать работу**.

## Языковая политика

Чтобы проект оставался доступным для международных участников, **все Issues, Pull Requests, сообщения коммитов и комментарии в коде должны быть написаны на английском языке.**

## Работа с ветками

В основном репозитории поддерживаются только две ветки:

- **main**: стабильная версия проекта. Всегда содержит готовый к использованию код.
- **dev**: ветка активной разработки. Здесь создаются и тестируются новые фичи для следующего релиза. После завершения разработки фичи, **dev** вливается в **main** - выпускается MINOR-релиз.

Все остальные ветки (`feature/...`, `fix/...`) создаются **в вашем форке**, когда вы работаете над задачей или багфиксом:

- **feature/...**: ветки для новых функций (создаются от `dev`). После завершения работы создается PR в `dev`.
- **fix/...**: ветки для исправления багов (создаются от `main`). После завершения работы создается PR в `main`. После мержа в `main` багфикс также мержится в `dev`, чтобы изменения попали в разрабатываемую версию.

## Правила оформления кода

Так же немало важно объявить стиль используемый в проекте. 
Решения, представленные в разделах ниже основываются на общепринятых правилах, используемых в Qt
Так как большая доля проекта использует данный фреймворк. Разность стиля в коде
создает путаницу и мешает дальнейшему анализу и разработке

Во вложенных разделах будут использоваться части проекта, для наглядности

### Комментарии

Вместо однострочных комментариев (`//`) принято использовать многострочные, как показано в листинге ниже.
Обратите внимание, что комментарии обязаны быть написаны на английском языке.

```cpp
/* Window setup */
this->setWindowState(Qt::WindowMaximized);
this->setWindowTitle("Cremniy"); /* <-- correct */

this->setWindowTitle("") // incorrect
```

Формат многострочных комментариев обычно исправляет редактор кода, тем не менее,
последующие строки должны идти ровно за предыдущей, и сопровождаться звездочкой в начале каждой строки комментария

Мы используем Doxygen, и вам советуем. Вот неплохой источник, объясняющий что это такое.
 - [Documenting C++ Code — LSST DM Developer Guide main...](https://developer.lsst.io/cpp/api-docs.html)  

```cpp
/**
 * @brief This is a correct doc comment 
 * @param projectPath Make sure, your stars follows one by one (and sleep well)
 */
IDEWindow::IDEWindow(const QString& projectPath, QWidget * parent) : QMainWindow(parent)
```

Далее показаны листинги как оформлять комментарии не допускается

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

### Определения классов, структур и других единиц

Обратите внимание, что:
 - Все объекты, что имеют тело, (например функция или условная конструкция), обязательно оформлятся в стиле K&R;
 - Все переменные, поля и функции, не смотря на свои модификаторы обязаны быть написаны в `camelCase`;
 - Классы, структуры, перечисления пишутся в `PascalCase` 

Венгерская нотация не допускается, за исключением объявлений закрытых полей внутри класса/структуры.

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

### Указатели и ссылки и огромные функции

Пожалуй это самое странное и несправедливое решение, которое стоит учесть.
Указатели всегда объявляются через звездочку, которая стоит ближе к типу данных.

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

Данное правило распространяются на ссылки и битовые поля.

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

Последнее правило, это вызов больших функций.
Функции, агрументы которой не умещаются в 80 символов или принимающие строго больше 3 агрументов, вызываются следующим образом
```cpp
/*correct Qt macro call*/
connect(
    this,
    &IDEWindow::saveFileSignal,
    m_filesTabWidget,
    &FilesTabWidget::saveFileSlot
);
```
Расстояние между положением названия и аргументами функции ровно один таб.

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
---
## Оформление текстовых элементов пользовательского интерфейса

Все видимые для пользователя элементы текста должны быть обёрнуты при помощи стандартных средств Qt:

- `tr()` – для обычных текстовых элементов, не являющихся константами и `static`.
- `QT_TRANSLATE_NOOP(context, text)` – объявление для Qt, чтобы текст попал в файл переводов.
- `QCoreApplication::translate(context, text)` – для статических констант, перевод будет применён сразу.
---

#### `tr()`

```c++
m_createFile = new QAction(tr("Create File"), this);
m_createDir  = new QAction(tr("Create Folder"), this);
m_delete     = new QAction(tr("Delete"), this);
m_rename     = new QAction(tr("Rename"), this);
m_open       = new QAction(tr("Open"), this);
```

---

#### `QT_TRANSLATE_NOOP(context, text)`

Используется для статических массивов и констант. Сам по себе не переводит – лишь помечает строку для `lupdate`. Перевод применяется позже через `QCoreApplication::translate()` или `tr()`.

> ⚠️ Контекст в `QT_TRANSLATE_NOOP` и `QCoreApplication::translate()` должен совпадать.

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

Если отрисовка происходит в том же классе, можно использовать `tr()` напрямую:
> Ключевой момент заключается в том, что контекст, который вы определяете для статических элементов, имеет то же имя, что и класс. В примере с `DataConverterDialog`, согласно стандарту Qt, имя контекста указывается именно таким образом.
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

---

#### `QMessageBox` с кастомными кнопками

Если вы определяете собственные кнопки – оборачивайте их текст в `tr()`:

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

---

#### Обновление файла переводов

После завершения работы с кодом запустите утилиту `lupdate`:

```bash
lupdate src -ts src/resources/locale/translations/app_ru.ts
```

Затем откройте файл перевода и заполните все поля со статусом `unfinished`:

```xml
<context>
    <name>QHexView</name> <-- Это ваш контекст
    <message>
        <location filename="../../../libs/HexEditor/src/qhexview.cpp" line="378"/> <-- Места где вы пометили строку как `tr()`
        <location filename="../../../libs/HexEditor/src/qhexview.cpp" line="397"/>
        <location filename="../../../libs/HexEditor/src/qhexview.cpp" line="405"/>
        <source>Go to</source> <-- текст который был помечен tr()
        <translation type="unfinished">тут должен быть перевод</translation>
    </message>
 </context>
```
---
## Pull requests

### Требования

- Pull request должен решать **одну конкретную задачу** или группу тесно связанных задач
- Не объединяйте в одном PR **разные изменения** (например: новые функции, рефакторинг и фиксы одновременно)
- Крупные изменения **разбивайте на несколько** отдельных PR
- Свяжите PR с задачей, если таковая существует ([ниже подробнее](CONTRIBUTING_ru.md#%D1%81%D0%B2%D1%8F%D0%B7%D1%8B%D0%B2%D0%B0%D0%BD%D0%B8%D0%B5-pr-%D1%81-%D0%B7%D0%B0%D0%B4%D0%B0%D1%87%D0%B0%D0%BC%D0%B8))

### Отправка

1. Сделайте fork репозитория
2. Создайте новую ветку от соответствующей базовой ветки:
   - `dev` для новых функций (feature)
   - `main` для исправления багов (fix)
3. Внесите ваши изменения
4. Синхронизируйте вашу ветку с базовой веткой (`dev` или `main`) и решите конфликты, если они есть
5. Создайте pull request в соответствующую ветку (`dev` или `main`) с понятным описанием или прикреплением Issue

### Связывание PR с задачами

Каждый Pull Request должен **явно указывать, какую задачу или Issue он решает**, если такая [задача](ROADMAP_ru.md) или Issue существует.
Если соответствующей задачи нет, просто опишите изменения в PR.

## Благодарность

Все контрибьюторы будут добавлены в [ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md)  
и упомянуты в конце каждого видео на [YouTube-канале](https://www.youtube.com/@igmunv)
