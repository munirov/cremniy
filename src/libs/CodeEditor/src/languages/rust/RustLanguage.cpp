#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

void registerRustLanguage()
{
    using namespace LanguageRuleHelpers;

    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("rust"),
        QStringLiteral("Rust"),
        {QStringLiteral("rs")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            QVector<RegexRule> rules = literalRules();
            rules += wordRules({
                QStringLiteral("as"), QStringLiteral("break"), QStringLiteral("const"), QStringLiteral("continue"),
                QStringLiteral("crate"), QStringLiteral("dyn"), QStringLiteral("else"), QStringLiteral("enum"),
                QStringLiteral("extern"), QStringLiteral("fn"), QStringLiteral("for"), QStringLiteral("if"),
                QStringLiteral("impl"), QStringLiteral("in"), QStringLiteral("let"), QStringLiteral("loop"),
                QStringLiteral("match"), QStringLiteral("mod"), QStringLiteral("move"), QStringLiteral("mut"),
                QStringLiteral("pub"), QStringLiteral("ref"), QStringLiteral("return"), QStringLiteral("self"),
                QStringLiteral("Self"), QStringLiteral("static"), QStringLiteral("struct"), QStringLiteral("super"),
                QStringLiteral("trait"), QStringLiteral("type"), QStringLiteral("unsafe"), QStringLiteral("use"),
                QStringLiteral("where"), QStringLiteral("while"), QStringLiteral("async"), QStringLiteral("await"),
                QStringLiteral("true"), QStringLiteral("false")
            }, QStringLiteral("Keyword"));
            rules += wordRules({
                QStringLiteral("i8"), QStringLiteral("i16"), QStringLiteral("i32"), QStringLiteral("i64"),
                QStringLiteral("i128"), QStringLiteral("isize"), QStringLiteral("u8"), QStringLiteral("u16"),
                QStringLiteral("u32"), QStringLiteral("u64"), QStringLiteral("u128"), QStringLiteral("usize"),
                QStringLiteral("f32"), QStringLiteral("f64"), QStringLiteral("bool"), QStringLiteral("char"),
                QStringLiteral("str"), QStringLiteral("String"), QStringLiteral("Vec"), QStringLiteral("Option"),
                QStringLiteral("Result")
            }, QStringLiteral("PrimitiveType"));
            rules.append({QRegularExpression(QStringLiteral("#!?\\[[^\\]]*\\]")), QStringLiteral("Preprocessor")});
            rules.append({QRegularExpression(QStringLiteral("\\b[a-z_][a-zA-Z0-9_]*!")), QStringLiteral("Function")});

            return new RuleBasedHighlighter(rules, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerRustLanguage)
