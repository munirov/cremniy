#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerYamlLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("yaml"),
        QStringLiteral("YAML"),
        {QStringLiteral("yaml"), QStringLiteral("yml")},
        {},
        QStringLiteral("#"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = wordRules({
                QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"), QStringLiteral("yes"),
                QStringLiteral("no"), QStringLiteral("on"), QStringLiteral("off")
            }, QStringLiteral("Keyword"));
            rules.append({QRegularExpression(QStringLiteral("^\\s*[^:#\\n]+:(?=\\s|$)")), QStringLiteral("Function")});

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("#[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerYamlLanguage)
