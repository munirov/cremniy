#include "languages/LanguageRegistry.h"

#include "QCXXHighlighter.hpp"

namespace {

void registerCppLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("cpp"),
        QStringLiteral("C++"),
        {QStringLiteral("cpp"), QStringLiteral("hpp"), QStringLiteral("cc"), QStringLiteral("cxx"),
         QStringLiteral("hh"), QStringLiteral("hxx")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* { return new QCXXHighlighter(document); }
    });

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("c"),
        QStringLiteral("C"),
        {QStringLiteral("c"), QStringLiteral("h")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* { return new QCXXHighlighter(document); }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerCppLanguage)
