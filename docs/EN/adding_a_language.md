<div align="center">

English • [Русский](adding_a_language_ru.md)

</div>

# Adding a New Language

## Overview

Syntax highlighting for every language in the editor goes through a single
interface: `LanguageRegistry`. Each language lives in its own directory
under `src/libs/CodeEditor/src/languages/<name>/`, with its own
`CMakeLists.txt` — the same "one directory, self-contained" pattern used by
`src/ui/MenuBar/Menus/`. Adding a new language means creating **one new
directory** (one `.cpp` file, one `CMakeLists.txt`), and adding **one line**
to `LanguageRegistration.cpp` (a linker-visibility requirement, explained
below). You never touch `src/libs/CodeEditor/CMakeLists.txt`,
`CustomCodeEditor.cpp`, `EditorLanguageSupport`, or any other existing
language's files.

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

### 2. Create the language directory and file

Create a new directory `src/libs/CodeEditor/src/languages/<name>/`
(lowercase, matching the language's `id`) and, inside it,
`<Name>Language.cpp`. For "Zig-like" that's
`src/libs/CodeEditor/src/languages/zig/ZigLanguage.cpp`. Here is a
complete, working example for a rule-based language:

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

If a language needs more than one closely related `LanguageDefinition`
(like `make/MakeLanguage.cpp`, which registers both "make" and "cmake"),
it's fine to register several from the same file and the same
`CREMNIY_REGISTER_LANGUAGE` call — see that file for the pattern. Prefer
this only when the definitions are genuinely inseparable (same file
historically, same tooling domain); otherwise each language gets its own
directory, even closely related ones — see `java/`, `csharp/`, `go/`, and
`php/` for four languages that share highlighting logic (via
`LanguageRuleHelpers::keywordsAndTypesRules`) but still live apart.

If your language needs its own `QStyleSyntaxHighlighter` subclass (option B
above), put its `.h`/`.cpp` in the same directory as the language file —
see `asm/AsmHighlighter.*`, `make/MakefileHighlighter.*`, or
`markdown/MarkdownHighlighter.*` for examples. Generic engines reused by
several languages live elsewhere (see "Where Things Live" below), and are
reached with a directory-qualified include, e.g.
`#include "core/RuleBasedHighlighter.h"`.

### 3. Register the directory in CMake

Create `src/libs/CodeEditor/src/languages/<name>/CMakeLists.txt`:

```cmake
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ZigLanguage.cpp
)
```

If your language also has its own highlighter class, list its `.h`/`.cpp`
here too (see `asm/CMakeLists.txt` for an example with a highlighter).

That's it — `src/libs/CodeEditor/src/languages/CMakeLists.txt` globs every
subdirectory here and calls `add_subdirectory()` on it automatically, the
same pattern used by `src/ui/MenuBar/Menus/`. You never edit
`src/libs/CodeEditor/CMakeLists.txt` or the top-level `languages/`
`CMakeLists.txt` to add a language.

### 4. Add one line to LanguageRegistration.cpp

Because `CodeEditor` is built as a **static library**, a linker is free to
drop an object file entirely if nothing in the final binary references any
symbol from it — and a language's `.cpp` file, on its own, exposes nothing
that's called by name anywhere else. `CREMNIY_REGISTER_LANGUAGE` generates
an externally-linked `<registerFn>_forceLink()` thunk specifically to give
the linker a reason to keep the file; `LanguageRegistration.cpp` is the one
place that calls every language's thunk explicitly, and `main()` calls
`registerAllLanguages()` once at startup.

Open `src/libs/CodeEditor/src/languages/core/LanguageRegistration.cpp` and
add two lines: an `extern` declaration alongside the others, and a call
inside `registerAllLanguages()`:

```cpp
extern void registerYamlLanguage_forceLink();
extern void registerZigLanguage_forceLink();   // <- add this line

void registerAllLanguages()
{
    // ...
    registerYamlLanguage_forceLink();
    registerZigLanguage_forceLink();   // <- add this line
}
```

The function name is always `<the name you passed to
CREMNIY_REGISTER_LANGUAGE>_forceLink` — for `registerZigLanguage`, that's
`registerZigLanguage_forceLink`.

**If you skip this step:** the project still compiles and links
successfully, but on some linkers/platforms your language simply won't
appear at runtime — no error, no warning, just silently no highlighting.
This is exactly the failure mode that motivated adding this step; see the
comments in `LanguageRegistry.h` and `LanguageRegistration.cpp` for the
full explanation if you're curious.

### 5. Build and verify

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
#include "xml/XmlLanguageHighlighter.h"

// ...
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

- **Forgetting the `CMakeLists.txt` file (or the `target_sources` line in
  it).** The file simply won't be compiled and the language won't appear —
  no error, no warning.
- **Forgetting the `LanguageRegistration.cpp` entry.** The project still
  builds fine, but the language silently doesn't register at runtime on
  linkers that drop unreferenced object files from a static library — see
  step 4 above.
- **Duplicate `id`.** Re-registering an existing id silently overwrites the
  previous definition. Check `LanguageRegistry::allLanguages()` (or just
  grep `src/libs/CodeEditor/src/languages/`) before picking an id.
- **Forgetting `nullptr`-safety in `createHighlighter`.** If your lambda can
  throw or fail to construct a highlighter, return `nullptr` explicitly
  rather than leaving undefined behavior — the caller checks for it.
- **Case-sensitive `exactFileNames`.** Don't add `"Makefile"` and
  `"makefile"` as two separate entries — matching is already
  case-insensitive; one entry ("Makefile") is enough.

## Where Things Live

| What                                      | Where |
|--------------------------------------------|-------|
| Interface definition                       | `src/libs/CodeEditor/include/languages/LanguageDefinition.h` |
| Registry + registration macro              | `src/libs/CodeEditor/include/languages/LanguageRegistry.h`, `src/libs/CodeEditor/src/languages/core/LanguageRegistry.cpp` |
| Force-link hook (add one line here too)    | `src/libs/CodeEditor/src/languages/core/LanguageRegistration.cpp` |
| Rule-building helpers                      | `src/libs/CodeEditor/include/languages/LanguageRuleHelpers.h` |
| Existing per-language directories (examples) | `src/libs/CodeEditor/src/languages/<name>/` |
| Generic rule-based highlighter             | `src/libs/CodeEditor/src/languages/core/RuleBasedHighlighter.*` |
| Legacy XML-resource highlighter            | `src/libs/CodeEditor/src/languages/xml/XmlLanguageHighlighter.*` |
| Language-directory glob (rarely touched)   | `src/libs/CodeEditor/src/languages/CMakeLists.txt` |
| Build file to update                       | never — create `src/libs/CodeEditor/src/languages/<name>/CMakeLists.txt` instead |
