#include "languages/LanguageRegistry.h"

#include "highlighters/XmlLanguageHighlighter.h"

#include <QRegularExpression>

namespace {

void registerPythonLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("python"),
        QStringLiteral("Python"),
        {QStringLiteral("py")},
        {},
        QStringLiteral("#"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return new XmlLanguageHighlighter(
                QStringLiteral(":/languages/python.xml"),
                QRegularExpression(QStringLiteral("#[^\\n]*")),
                QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
                document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerPythonLanguage)
