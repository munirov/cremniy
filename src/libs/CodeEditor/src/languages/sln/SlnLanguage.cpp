#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerSlnLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("sln"),
        QStringLiteral("Visual Studio Solution"),
        {QStringLiteral("sln")},
        {},
        QStringLiteral("#"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = wordRules({
                QStringLiteral("Project"), QStringLiteral("EndProject"), QStringLiteral("Global"),
                QStringLiteral("EndGlobal"), QStringLiteral("GlobalSection"), QStringLiteral("EndGlobalSection")
            }, QStringLiteral("Keyword"));
            rules.append({QRegularExpression(QStringLiteral("\"[^\"]+\"")), QStringLiteral("String")});

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("^#.*$")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerSlnLanguage)
