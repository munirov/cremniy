#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerPhpLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("php"),
        QStringLiteral("PHP"),
        {QStringLiteral("php")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = keywordsAndTypesRules({
                QStringLiteral("class"), QStringLiteral("function"), QStringLiteral("public"), QStringLiteral("private"),
                QStringLiteral("protected"), QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("elseif"),
                QStringLiteral("return"), QStringLiteral("foreach"), QStringLiteral("while"), QStringLiteral("namespace"),
                QStringLiteral("use"), QStringLiteral("extends"), QStringLiteral("implements"), QStringLiteral("trait"),
                QStringLiteral("static"), QStringLiteral("new")
            }, {
                QStringLiteral("int"), QStringLiteral("float"), QStringLiteral("bool"), QStringLiteral("string"),
                QStringLiteral("array"), QStringLiteral("void")
            });

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerPhpLanguage)
