#include "languages/LanguageRegistry.h"

#include "highlighters/MarkdownHighlighter.h"

namespace {

void registerMarkdownLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("markdown"),
        QStringLiteral("Markdown"),
        {QStringLiteral("md"), QStringLiteral("markdown")},
        {},
        QString(), // Markdown doesn't have a conventional line comment syntax.
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* { return new MarkdownHighlighter(document); }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerMarkdownLanguage)
