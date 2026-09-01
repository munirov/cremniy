<div align="center">

[English](adding_a_language.md) • Русский

</div>

# Добавление нового языка

## Обзор

Подсветка синтаксиса для всех языков в редакторе идёт через единый
интерфейс — `LanguageRegistry`. Каждый язык живёт в своей директории
`src/libs/CodeEditor/src/languages/<name>/` со своим `CMakeLists.txt` — по
тому же принципу "одна директория — всё самодостаточно", что и в
`src/ui/MenuBar/Menus/`. Добавление нового языка сводится к созданию
**одной новой директории** (один `.cpp`-файл, один `CMakeLists.txt`) и
добавлению **одной строки** в `LanguageRegistration.cpp` (требование,
связанное с видимостью символов для линкера — объяснено ниже). Трогать
`src/libs/CodeEditor/CMakeLists.txt`, `CustomCodeEditor.cpp`,
`EditorLanguageSupport` или файлы любого другого языка не нужно.

В этом документе процесс разобран на примере вымышленного языка
"Zig-подобный".

## Интерфейс

Всё, что языку нужно рассказать о себе, умещается в одной структуре —
`LanguageDefinition` (`src/libs/CodeEditor/include/languages/LanguageDefinition.h`):

```cpp
struct LanguageDefinition {
    QString id;                      // уникальный ключ, например "python"
    QString displayName;             // имя для UI, например "Python"
    QStringList fileExtensions;      // {"py"}
    QStringList exactFileNames;      // {"Makefile", "Dockerfile"} — без учёта регистра
    QString lineCommentPrefix;       // "#", "//", ";" или "" если комментариев нет
    std::function<QStyleSyntaxHighlighter*(QTextDocument*)> createHighlighter;
};
```

Язык регистрирует один (или несколько) экземпляров `LanguageDefinition`
через `LanguageRegistry::instance().registerLanguage(...)` — обычно из
статической функции регистрации, вызываемой макросом
`CREMNIY_REGISTER_LANGUAGE` на уровне файла. Регистрация происходит один
раз, автоматически, до запуска `main()` — больше ничего вызывать не нужно.

## Пошагово: добавляем язык

### 1. Реши, как будет подсвечиваться язык

Два варианта, в порядке предпочтения:

**А. На основе правил (подходит для большинства языков).** Обычно
достаточно набора регулярных выражений, сопоставленных с именами форматов
(`Keyword`, `String`, `Number`, `Function`, `Type`, `Comment`,
`Preprocessor` и т.д.). Используй `RuleBasedHighlighter` вместе с
хелперами из `LanguageRuleHelpers.h` (`wordRules`, `stringLiteralRule`,
`numberLiteralRule`, `literalRules`).

**Б. Собственный класс-наследник `QStyleSyntaxHighlighter`.** Нужен только
для языков, чью логику подсветки реально невозможно выразить как
regex-правила применительно к одной строке (например, многострочные
блочные комментарии со сложным состоянием). Существующие примеры:
`AsmHighlighter`, `MarkdownHighlighter`, `MakefileHighlighter`.

Почти для любого нового языка правильный выбор — вариант А.

### 2. Создай директорию и файл языка

Создай новую директорию `src/libs/CodeEditor/src/languages/<name>/`
(в нижнем регистре, совпадает с `id` языка), а внутри неё —
`<Name>Language.cpp`. Для "Zig-подобного" это
`src/libs/CodeEditor/src/languages/zig/ZigLanguage.cpp`. Вот полный
рабочий пример для языка на правилах:

```cpp
#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerZigLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("zig"),
        QStringLiteral("Zig"),
        {QStringLiteral("zig")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = literalRules(); // строки + числа
            rules += wordRules({
                QStringLiteral("const"), QStringLiteral("var"), QStringLiteral("fn"),
                QStringLiteral("pub"), QStringLiteral("return"), QStringLiteral("if"),
                QStringLiteral("else"), QStringLiteral("while"), QStringLiteral("for"),
                QStringLiteral("struct"), QStringLiteral("enum"), QStringLiteral("union")
            }, QStringLiteral("Keyword"));
            rules += wordRules({
                QStringLiteral("i32"), QStringLiteral("u32"), QStringLiteral("f64"),
                QStringLiteral("bool"), QStringLiteral("void")
            }, QStringLiteral("PrimitiveType"));

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerZigLanguage)
```

Разбор по полям:

- **`id`** — в нижнем регистре, без пробелов. Используется внутренне
  (`syntaxKey()`, определение по расширению). Должен быть уникален в
  пределах всего реестра.
- **`displayName`** — показывается в статус-баре / будущем меню "Язык".
- **`fileExtensions`** — без точки в начале, в нижнем регистре (сравнение
  всё равно не зависит от регистра). Первое расширение ничем не выделено —
  перечисли все расширения, которые должны относиться к этому языку.
- **`exactFileNames`** — для файлов, определяемых по имени, а не по
  расширению (`Makefile`, `Dockerfile`, `CMakeLists.txt`, `.gitignore`).
  Сравнение не зависит от регистра.
- **`lineCommentPrefix`** — используется действием "закомментировать
  строку". Пустая строка (`QString()`), если у языка нет однострочных
  комментариев (например, JSON, XML).
- **`createHighlighter`** — лямбда-фабрика. Обязана вернуть выделенный в
  куче `QStyleSyntaxHighlighter*`, который получает владение через параметр
  `document` (стандартное поведение `QSyntaxHighlighter`), либо `nullptr`,
  если язык осознанно не подсвечивается.

Если языку нужно больше одного тесно связанного `LanguageDefinition` (как
`make/MakeLanguage.cpp`, который регистрирует и "make", и "cmake"), можно
зарегистрировать несколько из одного файла и одного вызова
`CREMNIY_REGISTER_LANGUAGE` — см. этот файл как образец. Делай так только
если определения реально неразделимы (исторически один файл, одна область
инструментов); в остальных случаях у каждого языка своя директория, даже у
близкородственных — смотри `java/`, `csharp/`, `go/` и `php/`: четыре языка
делят логику подсветки (через
`LanguageRuleHelpers::keywordsAndTypesRules`), но всё равно живут раздельно.

Если языку нужен собственный класс-наследник `QStyleSyntaxHighlighter`
(вариант Б выше), помести его `.h`/`.cpp` в ту же директорию, что и файл
языка — см. `asm/AsmHighlighter.*`, `make/MakefileHighlighter.*` или
`markdown/MarkdownHighlighter.*` как примеры. Универсальные движки,
переиспользуемые несколькими языками, живут отдельно (см. "Где что лежит"
ниже) и подключаются через путь с директорией, например
`#include "core/RuleBasedHighlighter.h"`.

### 3. Зарегистрируй директорию в CMake

Создай `src/libs/CodeEditor/src/languages/<name>/CMakeLists.txt`:

```cmake
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ZigLanguage.cpp
)
```

Если у языка есть свой класс хайлайтера, добавь его `.h`/`.cpp` сюда же
(см. `asm/CMakeLists.txt` — пример с хайлайтером).

Это всё — `src/libs/CodeEditor/src/languages/CMakeLists.txt` сам
перебирает все поддиректории и вызывает для каждой `add_subdirectory()`,
по тому же принципу, что и `src/ui/MenuBar/Menus/`. Ни
`src/libs/CodeEditor/CMakeLists.txt`, ни корневой `CMakeLists.txt` в
`languages/` для добавления языка редактировать не нужно.

### 4. Добавь одну строку в LanguageRegistration.cpp

Так как `CodeEditor` собирается как **статическая библиотека**, линкер
вправе полностью выбросить object-файл, если ни один символ из него не
используется где-либо в финальном бинарнике — а `.cpp`-файл языка сам по
себе ничего не экспортирует, на что можно было бы сослаться по имени.
`CREMNIY_REGISTER_LANGUAGE` генерирует функцию с внешним связыванием
`<registerFn>_forceLink()` именно для того, чтобы дать линкеру повод
оставить файл; `LanguageRegistration.cpp` — единственное место, где явно
вызываются thunk-функции всех языков, а `main()` один раз при старте
вызывает `registerAllLanguages()`.

Открой `src/libs/CodeEditor/src/languages/core/LanguageRegistration.cpp` и
добавь две строки: `extern`-объявление рядом с остальными и вызов внутри
`registerAllLanguages()`:

```cpp
extern void registerYamlLanguage_forceLink();
extern void registerZigLanguage_forceLink();   // <- добавь эту строку

void registerAllLanguages()
{
    // ...
    registerYamlLanguage_forceLink();
    registerZigLanguage_forceLink();   // <- добавь эту строку
}
```

Имя функции всегда `<имя, переданное в CREMNIY_REGISTER_LANGUAGE>_forceLink`
— для `registerZigLanguage` это `registerZigLanguage_forceLink`.

**Если пропустить этот шаг:** проект всё равно успешно соберётся и
слинкуется, но на некоторых линковщиках/платформах язык просто не появится
в рантайме — без единой ошибки или предупреждения, просто молча без
подсветки. Это ровно та проблема, из-за которой этот шаг и появился; за
подробным объяснением — комментарии в `LanguageRegistry.h` и
`LanguageRegistration.cpp`.

### 5. Собери и проверь

После сборки открой (или создай) файл с новым расширением. Должно
появиться:

- корректная подсветка синтаксиса
- правильное имя языка в статус-баре
- `Ctrl+/` (или твоя горячая клавиша закомментирования) использует
  правильный префикс комментария

## Переиспользование существующего XML-ресурса языка

Несколько языков (Python, Lua, GLSL, CMake) до сих пор переиспользуют
XML-определения из стороннего компонента `QCodeEditor`
(`resources/languages/*.xml`, скомпилированные в Qt-ресурс
`:/languages/*.xml`) через `XmlLanguageHighlighter`. Это устаревший путь,
оставленный для языков, у которых уже были хорошие XML-определения; для
новых языков он **не обязателен**, и подсветка на основе правил (вариант А
выше) предпочтительнее для всего нового. Используй этот способ, только
если осознанно переиспользуешь один из существующих `.xml`-ресурсов.

```cpp
#include "xml/XmlLanguageHighlighter.h"

// ...
[](QTextDocument* document) -> QStyleSyntaxHighlighter* {
    return new XmlLanguageHighlighter(
        QStringLiteral(":/languages/mylang.xml"),
        QRegularExpression(QStringLiteral("#[^\\n]*")),       // шаблон комментария
        QRegularExpression(QStringLiteral("\"[^\"\\n]*\"")),  // шаблон строки
        QRegularExpression(QStringLiteral("\\b\\d+\\b")),     // шаблон числа
        document);
}
```

## Частые ошибки

- **Забыть файл `CMakeLists.txt` (или строку `target_sources` в нём).**
  Файл просто не скомпилируется, и язык не появится — без ошибок и
  предупреждений.
- **Забыть строку в `LanguageRegistration.cpp`.** Проект соберётся и
  слинкуется без ошибок, но язык молча не зарегистрируется в рантайме на
  линковщиках, отбрасывающих неиспользуемые object-файлы из статической
  библиотеки — см. шаг 4 выше.
- **Дублирующийся `id`.** Повторная регистрация существующего id молча
  перезаписывает предыдущее определение. Перед выбором id проверь
  `LanguageRegistry::allLanguages()` (или просто поищи grep'ом по
  `src/libs/CodeEditor/src/languages/`).
- **Забыть про безопасность `nullptr` в `createHighlighter`.** Если лямбда
  может не суметь создать хайлайтер, верни `nullptr` явно, а не оставляй
  неопределённое поведение — вызывающий код это проверяет.
- **Регистрозависимые `exactFileNames`.** Не добавляй `"Makefile"` и
  `"makefile"` как две разные записи — сравнение и так не зависит от
  регистра; одной записи ("Makefile") достаточно.

## Где что лежит

| Что                                          | Где |
|------------------------------------------------|-----|
| Определение интерфейса                         | `src/libs/CodeEditor/include/languages/LanguageDefinition.h` |
| Реестр + макрос регистрации                     | `src/libs/CodeEditor/include/languages/LanguageRegistry.h`, `src/libs/CodeEditor/src/languages/core/LanguageRegistry.cpp` |
| Хук принудительной линковки (тоже добавь строку) | `src/libs/CodeEditor/src/languages/core/LanguageRegistration.cpp` |
| Хелперы построения правил                       | `src/libs/CodeEditor/include/languages/LanguageRuleHelpers.h` |
| Существующие директории языков (примеры)        | `src/libs/CodeEditor/src/languages/<name>/` |
| Универсальный хайлайтер на правилах             | `src/libs/CodeEditor/src/languages/core/RuleBasedHighlighter.*` |
| Устаревший XML-ресурсный хайлайтер              | `src/libs/CodeEditor/src/languages/xml/XmlLanguageHighlighter.*` |
| Общий glob по директориям языков (менять редко) | `src/libs/CodeEditor/src/languages/CMakeLists.txt` |
| Файл сборки для обновления                      | никогда — вместо этого создай `src/libs/CodeEditor/src/languages/<name>/CMakeLists.txt` |
