<div align="center">

[English](adding_a_language.md) • Русский

</div>

# Добавление нового языка

## Обзор

Подсветка синтаксиса для всех языков в редакторе идёт через единый
интерфейс — `LanguageRegistry`. Добавление нового языка сводится к
созданию **одного нового `.cpp` файла** и добавлению **одной строки** в
`CMakeLists.txt`. Трогать `CustomCodeEditor.cpp`, `EditorLanguageSupport`
или любой другой существующий файл не нужно.

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

### 2. Создай файл языка

Создай `src/libs/CodeEditor/src/languages/<Name>Language.cpp`. Вот полный
рабочий пример для языка на правилах ("Zig-подобный"):

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

Если нужно добавить несколько близкородственных языков (как в существующем
`CBraceFamilyLanguages.cpp` для Java/C#/Go/PHP), можно зарегистрировать
несколько `LanguageDefinition` из одного файла и одного вызова
`CREMNIY_REGISTER_LANGUAGE` — см. этот файл как образец.

### 3. Зарегистрируй файл в CMake

Открой `src/libs/CodeEditor/CMakeLists.txt` и добавь новый файл в список
`add_library(CodeEditor STATIC ...)`, рядом с остальными файлами из
`src/languages/`:

```cmake
    src/languages/YamlLanguage.cpp
    src/languages/ZigLanguage.cpp   # <- добавь эту строку
```

Это всё. Больше никакой файл менять не нужно.

### 4. Собери и проверь

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

- **Забыть строку в `CMakeLists.txt`.** Файл просто не скомпилируется, и
  язык не появится — без ошибок и предупреждений.
- **Дублирующийся `id`.** Повторная регистрация существующего id молча
  перезаписывает предыдущее определение. Перед выбором id проверь
  `LanguageRegistry::allLanguages()` (или просто поищи grep'ом по
  `src/languages/`).
- **Забыть про безопасность `nullptr` в `createHighlighter`.** Если лямбда
  может не суметь создать хайлайтер, верни `nullptr` явно, а не оставляй
  неопределённое поведение — вызывающий код это проверяет.
- **Регистрозависимые `exactFileNames`.** Не добавляй `"Makefile"` и
  `"makefile"` как две разные записи — сравнение и так не зависит от
  регистра; одной записи ("Makefile") достаточно.

## Где что лежит

| Что                                       | Где |
|--------------------------------------------|-----|
| Определение интерфейса                     | `src/libs/CodeEditor/include/languages/LanguageDefinition.h` |
| Реестр + макрос регистрации                 | `src/libs/CodeEditor/include/languages/LanguageRegistry.h`, `src/languages/LanguageRegistry.cpp` |
| Хелперы построения правил                   | `src/libs/CodeEditor/include/languages/LanguageRuleHelpers.h` |
| Существующие файлы языков (примеры)         | `src/libs/CodeEditor/src/languages/*.cpp` |
| Универсальный хайлайтер на правилах         | `src/libs/CodeEditor/src/widgets/highlighters/RuleBasedHighlighter.*` |
| Устаревший XML-ресурсный хайлайтер          | `src/libs/CodeEditor/src/widgets/highlighters/XmlLanguageHighlighter.*` |
| Файл сборки для обновления                  | `src/libs/CodeEditor/CMakeLists.txt` |
