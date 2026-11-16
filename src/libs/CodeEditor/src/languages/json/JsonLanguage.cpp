#include "languages/LanguageRegistry.h"

#include "QJSONHighlighter.hpp"

namespace {

void registerJsonLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("json"),
        QStringLiteral("JSON"),
        {QStringLiteral("json")},
        {},
        QString(), // JSON has no comment syntax.
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* { return new QJSONHighlighter(document); }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerJsonLanguage)
