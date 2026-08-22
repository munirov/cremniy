<div align="center">

English • [Русский](adding_a_language_ru.md)

</div>

# Adding a New Language

## Overview

Syntax highlighting for every language in the editor goes through a single
interface: `LanguageRegistry`. Adding a new language means creating **one
new `.cpp` file** and adding **one line** to `CMakeLists.txt`. You do not
need to touch `CustomCodeEditor.cpp`, `EditorLanguageSupport`, or any other
existing file.

This document walks through the whole process using a fictional "Zig-like"
language as an example.

## The Interface

Everything a language needs to describe itself lives in one struct,
`LanguageDefinition` (`src/libs/CodeEditor/include/languages/LanguageDefinition.h`):

```cpp
struct LanguageDefinition {
    QString id;                      // unique key, e.g. "python"
    QString displayName;             // UI label, e.g. "Python"
    QStringList fileExtensions;      // {"py"}
    QStringList exactFileNames;      // {"Makefile", "Dockerfile"} — case-insensitive
    QString lineCommentPrefix;       // "#", "//", ";", or "" if none
    std::function<QStyleSyntaxHighlighter*(QTextDocument*)> createHighlighter;
};
```

A language registers one (or more) `LanguageDefinition` instances with
`LanguageRegistry::instance().registerLanguage(...)`, typically from a
static registration function triggered by the `CREMNIY_REGISTER_LANGUAGE`
macro at file scope. Registration happens once, automatically, before
`main()` runs — nothing else needs to call anything.

## Step-by-Step: Adding a Language

### 1. Decide how the language will be highlighted

Two options, in order of preference:

**A. Rule-based (most languages should use this).** A list of regular
expressions mapped to format names (`Keyword`, `String`, `Number`,
`Function`, `Type`, `Comment`, `Preprocessor`, ...) is usually enough. Use
`RuleBasedHighlighter` plus the helpers in `LanguageRuleHelpers.h`
(`wordRules`, `stringLiteralRule`, `numberLiteralRule`, `literalRules`).

**B. A custom `QStyleSyntaxHighlighter` subclass.** Only needed for
languages with highlighting logic that genuinely can't be expressed as
regex rules against a single line (multi-line block comments with nested
state, for instance). Existing examples: `AsmHighlighter`,
`MarkdownHighlighter`, `MakefileHighlighter`.

For almost every new language, option A is the right choice.

### 2. Create the language file

Create `src/libs/CodeEditor/src/languages/<Name>Language.cpp`. Here is a
complete, working example for a rule-based language ("Zig-like"):

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
            QVector<RegexRule> rules = literalRules(); // strings + numbers
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

Field-by-field:

- **`id`** — lowercase, no spaces. Used internally (`syntaxKey()`,
  extension resolution). Must be unique across the whole registry.
- **`displayName`** — shown in the status bar / any future "Language" menu.
- **`fileExtensions`** — no leading dot, lowercase (matching is
  case-insensitive regardless). The first one is not treated specially —
  list all extensions that should map to this language.
- **`exactFileNames`** — for files identified by name rather than
  extension (`Makefile`, `Dockerfile`, `CMakeLists.txt`, `.gitignore`).
  Matching is case-insensitive.
- **`lineCommentPrefix`** — used by the "toggle line comment" action. Use
  an empty string (`QString()`) if the language has no single-line comment
  syntax (e.g. JSON, XML).
- **`createHighlighter`** — a factory lambda/function pointer. Must return
  a heap-allocated `QStyleSyntaxHighlighter*` that takes ownership via the
  `document` parameter (standard `QSyntaxHighlighter` behavior), or
  `nullptr` if the language is intentionally unhighlighted.

If you need multiple closely related languages (like the existing
`CBraceFamilyLanguages.cpp` for Java/C#/Go/PHP), it's fine to register
several `LanguageDefinition`s from the same file and the same
`CREMNIY_REGISTER_LANGUAGE` call — see that file for the pattern.

### 3. Register the file in CMake

Open `src/libs/CodeEditor/CMakeLists.txt` and add your new file to the
`add_library(CodeEditor STATIC ...)` list, next to the other files under
`src/languages/`:

```cmake
    src/languages/YamlLanguage.cpp
    src/languages/ZigLanguage.cpp   # <- add this line
```

That's it. No other file needs to change.

### 4. Build and verify

After building, open (or create) a file with your new extension. You
should see:

- correct syntax highlighting
- the right language name in the status bar
- `Ctrl+/` (or your comment-toggle shortcut) using the right comment prefix

## Reusing an Existing XML Language Resource

A few languages (Python, Lua, GLSL, CMake) still reuse XML-based language
definitions bundled with the third-party `QCodeEditor` component
(`resources/languages/*.xml`, compiled into the `:/languages/*.xml` Qt
resource path) via `XmlLanguageHighlighter`. This is a legacy path kept for
languages that already had good XML definitions; it is **not** required for
new languages, and rule-based highlighting (option A above) is preferred
for anything new. Use this only if you're deliberately reusing one of the
existing `.xml` resource files.

```cpp
[](QTextDocument* document) -> QStyleSyntaxHighlighter* {
    return new XmlLanguageHighlighter(
        QStringLiteral(":/languages/mylang.xml"),
        QRegularExpression(QStringLiteral("#[^\\n]*")),       // comment pattern
        QRegularExpression(QStringLiteral("\"[^\"\\n]*\"")),  // string pattern
        QRegularExpression(QStringLiteral("\\b\\d+\\b")),     // number pattern
        document);
}
```

## Common Mistakes

- **Forgetting the `CMakeLists.txt` line.** The file simply won't be
  compiled and the language won't appear — no error, no warning.
- **Duplicate `id`.** Re-registering an existing id silently overwrites the
  previous definition. Check `LanguageRegistry::allLanguages()` (or just
  grep `src/languages/`) before picking an id.
- **Forgetting `nullptr`-safety in `createHighlighter`.** If your lambda can
  throw or fail to construct a highlighter, return `nullptr` explicitly
  rather than leaving undefined behavior — the caller checks for it.
- **Case-sensitive `exactFileNames`.** Don't add `"Makefile"` and
  `"makefile"` as two separate entries — matching is already
  case-insensitive; one entry ("Makefile") is enough.

## Where Things Live

| What                                    | Where |
|------------------------------------------|-------|
| Interface definition                     | `src/libs/CodeEditor/include/languages/LanguageDefinition.h` |
| Registry + registration macro            | `src/libs/CodeEditor/include/languages/LanguageRegistry.h`, `src/languages/LanguageRegistry.cpp` |
| Rule-building helpers                    | `src/libs/CodeEditor/include/languages/LanguageRuleHelpers.h` |
| Existing per-language files (examples)   | `src/libs/CodeEditor/src/languages/*.cpp` |
| Generic rule-based highlighter           | `src/libs/CodeEditor/src/widgets/highlighters/RuleBasedHighlighter.*` |
| Legacy XML-resource highlighter          | `src/libs/CodeEditor/src/widgets/highlighters/XmlLanguageHighlighter.*` |
| Build file to update                     | `src/libs/CodeEditor/CMakeLists.txt` |
