#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerJavaLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("java"),
        QStringLiteral("Java"),
        {QStringLiteral("java")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = keywordsAndTypesRules({
                QStringLiteral("abstract"), QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
                QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("else"),
                QStringLiteral("enum"), QStringLiteral("extends"), QStringLiteral("finally"), QStringLiteral("for"),
                QStringLiteral("if"), QStringLiteral("implements"), QStringLiteral("import"), QStringLiteral("interface"),
                QStringLiteral("new"), QStringLiteral("package"), QStringLiteral("private"), QStringLiteral("protected"),
                QStringLiteral("public"), QStringLiteral("return"), QStringLiteral("static"), QStringLiteral("switch"),
                QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("while")
            }, {
                QStringLiteral("int"), QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("float"),
                QStringLiteral("double"), QStringLiteral("boolean"), QStringLiteral("char"), QStringLiteral("byte"),
                QStringLiteral("void"), QStringLiteral("String")
            });

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerJavaLanguage)
