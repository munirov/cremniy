#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerShellLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("shell"),
        QStringLiteral("Shell"),
        {QStringLiteral("sh"), QStringLiteral("bash"), QStringLiteral("zsh"), QStringLiteral("fish")},
        {QStringLiteral("Dockerfile")},
        QStringLiteral("#"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = literalRules();
            rules += wordRules({
                QStringLiteral("if"), QStringLiteral("then"), QStringLiteral("else"), QStringLiteral("elif"),
                QStringLiteral("fi"), QStringLiteral("for"), QStringLiteral("do"), QStringLiteral("done"),
                QStringLiteral("while"), QStringLiteral("case"), QStringLiteral("esac"), QStringLiteral("function"),
                QStringLiteral("in"), QStringLiteral("export"), QStringLiteral("local"), QStringLiteral("readonly")
            }, QStringLiteral("Keyword"));
            rules += wordRules({
                QStringLiteral("echo"), QStringLiteral("cd"), QStringLiteral("test"), QStringLiteral("printf"),
                QStringLiteral("source")
            }, QStringLiteral("Function"));

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("#[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerShellLanguage)
