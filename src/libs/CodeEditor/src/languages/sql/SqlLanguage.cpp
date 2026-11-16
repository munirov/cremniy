#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerSqlLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("sql"),
        QStringLiteral("SQL"),
        {QStringLiteral("sql")},
        {},
        QStringLiteral("--"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = wordRules({
                QStringLiteral("select"), QStringLiteral("from"), QStringLiteral("where"), QStringLiteral("insert"),
                QStringLiteral("into"), QStringLiteral("update"), QStringLiteral("delete"), QStringLiteral("join"),
                QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("inner"), QStringLiteral("outer"),
                QStringLiteral("group"), QStringLiteral("by"), QStringLiteral("order"), QStringLiteral("limit"),
                QStringLiteral("create"), QStringLiteral("table"), QStringLiteral("alter"), QStringLiteral("drop"),
                QStringLiteral("index"), QStringLiteral("values"), QStringLiteral("set"), QStringLiteral("and"),
                QStringLiteral("or"), QStringLiteral("not"), QStringLiteral("null")
            }, QStringLiteral("Keyword"));

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("--[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerSqlLanguage)
