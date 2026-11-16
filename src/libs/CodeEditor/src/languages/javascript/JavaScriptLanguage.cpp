#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

QStringList jsKeywords()
{
    return {
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("delete"),
        QStringLiteral("else"), QStringLiteral("export"), QStringLiteral("extends"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("from"), QStringLiteral("function"), QStringLiteral("if"),
        QStringLiteral("import"), QStringLiteral("in"), QStringLiteral("instanceof"), QStringLiteral("let"),
        QStringLiteral("new"), QStringLiteral("return"), QStringLiteral("super"), QStringLiteral("switch"),
        QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("typeof"),
        QStringLiteral("var"), QStringLiteral("while"), QStringLiteral("yield"), QStringLiteral("async"),
        QStringLiteral("await")
    };
}

QStyleSyntaxHighlighter* createJsLikeHighlighter(QTextDocument* document)
{
    using namespace LanguageRuleHelpers;

    QVector<RegexRule> rules = literalRules();
    rules += wordRules(jsKeywords(), QStringLiteral("Keyword"));
    rules += wordRules({
        QStringLiteral("string"), QStringLiteral("number"), QStringLiteral("boolean"), QStringLiteral("void"),
        QStringLiteral("null"), QStringLiteral("undefined"), QStringLiteral("any"), QStringLiteral("unknown"),
        QStringLiteral("never")
    }, QStringLiteral("PrimitiveType"));

    return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
}

void registerJavaScriptLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("javascript"),
        QStringLiteral("JavaScript"),
        {QStringLiteral("js"), QStringLiteral("mjs"), QStringLiteral("cjs"), QStringLiteral("jsx")},
        {},
        QStringLiteral("//"),
        &createJsLikeHighlighter
    });

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("typescript"),
        QStringLiteral("TypeScript"),
        {QStringLiteral("ts"), QStringLiteral("tsx")},
        {},
        QStringLiteral("//"),
        &createJsLikeHighlighter
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerJavaScriptLanguage)
