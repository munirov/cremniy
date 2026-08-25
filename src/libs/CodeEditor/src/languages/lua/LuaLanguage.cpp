#include "languages/LanguageRegistry.h"

#include "xml/XmlLanguageHighlighter.h"

#include <QRegularExpression>

namespace {

void registerLuaLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("lua"),
        QStringLiteral("Lua"),
        {QStringLiteral("lua")},
        {},
        QStringLiteral("--"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return new XmlLanguageHighlighter(
                QStringLiteral(":/languages/lua.xml"),
                QRegularExpression(QStringLiteral("--[^\\n]*")),
                QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")),
                QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
                document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerLuaLanguage)
