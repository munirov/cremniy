#include "languages/LanguageRegistry.h"

#include "highlighters/MakefileHighlighter.h"
#include "highlighters/XmlLanguageHighlighter.h"

#include <QRegularExpression>

namespace {

void registerMakeLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("make"),
        QStringLiteral("Makefile"),
        {QStringLiteral("mk"), QStringLiteral("make")},
        {QStringLiteral("Makefile"), QStringLiteral("GNUmakefile")},
        QStringLiteral("#"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* { return new MakefileHighlighter(document); }
    });

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("cmake"),
        QStringLiteral("CMake"),
        {QStringLiteral("cmake")},
        {QStringLiteral("CMakeLists.txt"), QStringLiteral("CMakeCache.txt")},
        QStringLiteral("#"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return new XmlLanguageHighlighter(
                QStringLiteral(":/languages/cmake.xml"),
                QRegularExpression(QStringLiteral("#[^\\n]*")),
                QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
                document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerMakeLanguage)
