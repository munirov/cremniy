#include "languages/LanguageRegistry.h"

#include "core/RuleBasedHighlighter.h"

#include <QRegularExpression>

namespace {

void registerXmlLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("xml"),
        QStringLiteral("XML"),
        {QStringLiteral("xml"), QStringLiteral("xaml"), QStringLiteral("svg"), QStringLiteral("vcxproj"),
         QStringLiteral("vcproj"), QStringLiteral("csproj"), QStringLiteral("fsproj"), QStringLiteral("props"),
         QStringLiteral("targets"), QStringLiteral("filters")},
        {},
        QString(), // XML comments (<!-- -->) aren't single-line prefix comments.
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = {
                {QRegularExpression(QStringLiteral("</?[A-Za-z_:][A-Za-z0-9:._-]*")), QStringLiteral("Keyword")},
                {QRegularExpression(QStringLiteral("\\b[A-Za-z_:][A-Za-z0-9:._-]*(?=\\=)")), QStringLiteral("Function")},
                {QRegularExpression(QStringLiteral("<!DOCTYPE[^>]*>|<\\?xml[^?]*\\?>")), QStringLiteral("Preprocessor")}
            };

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("<!--[^>]*-->")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerXmlLanguage)
