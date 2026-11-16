#include "languages/LanguageRegistry.h"

#include "xml/XmlLanguageHighlighter.h"

#include <QRegularExpression>

namespace {

void registerGlslLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("glsl"),
        QStringLiteral("GLSL"),
        {QStringLiteral("glsl"), QStringLiteral("vert"), QStringLiteral("frag")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return new XmlLanguageHighlighter(
                QStringLiteral(":/languages/glsl.xml"),
                QRegularExpression(QStringLiteral("//[^\\n]*")),
                QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
                document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerGlslLanguage)
