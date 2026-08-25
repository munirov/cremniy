#include "languages/LanguageRegistry.h"

#include "AsmHighlighter.h"

namespace {

void registerAsmLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("asm"),
        QStringLiteral("Assembly"),
        {QStringLiteral("asm"), QStringLiteral("s")},
        {},
        QStringLiteral(";"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* { return new AsmHighlighter(document); }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerAsmLanguage)
