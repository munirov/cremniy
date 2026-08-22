#include "languages/LanguageRegistry.h"
#include "languages/LanguageRuleHelpers.h"

#include <QRegularExpression>

namespace {

using namespace LanguageRuleHelpers;

QStyleSyntaxHighlighter* createRulesHighlighter(const QStringList& keywords, const QStringList& primitiveTypes,
                                                 const QRegularExpression& commentPattern, QTextDocument* document)
{
    QVector<RegexRule> rules = literalRules();
    rules += wordRules(keywords, QStringLiteral("Keyword"));
    rules += wordRules(primitiveTypes, QStringLiteral("PrimitiveType"));
    return new RuleBasedHighlighter(rules, commentPattern, document);
}

void registerJavaLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("java"),
        QStringLiteral("Java"),
        {QStringLiteral("java")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return createRulesHighlighter({
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
            }, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

void registerCSharpLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("csharp"),
        QStringLiteral("C#"),
        {QStringLiteral("cs")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return createRulesHighlighter({
                QStringLiteral("abstract"), QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
                QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("else"),
                QStringLiteral("enum"), QStringLiteral("extends"), QStringLiteral("finally"), QStringLiteral("for"),
                QStringLiteral("if"), QStringLiteral("implements"), QStringLiteral("import"), QStringLiteral("interface"),
                QStringLiteral("namespace"), QStringLiteral("new"), QStringLiteral("private"), QStringLiteral("protected"),
                QStringLiteral("public"), QStringLiteral("return"), QStringLiteral("static"), QStringLiteral("switch"),
                QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("try"), QStringLiteral("using"),
                QStringLiteral("while")
            }, {
                QStringLiteral("int"), QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("float"),
                QStringLiteral("double"), QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("byte"),
                QStringLiteral("void"), QStringLiteral("string")
            }, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

void registerGoLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("go"),
        QStringLiteral("Go"),
        {QStringLiteral("go")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return createRulesHighlighter({
                QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("chan"), QStringLiteral("const"),
                QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("defer"), QStringLiteral("else"),
                QStringLiteral("fallthrough"), QStringLiteral("for"), QStringLiteral("func"), QStringLiteral("go"),
                QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("import"), QStringLiteral("interface"),
                QStringLiteral("map"), QStringLiteral("package"), QStringLiteral("range"), QStringLiteral("return"),
                QStringLiteral("select"), QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("type"),
                QStringLiteral("var")
            }, {
                QStringLiteral("int"), QStringLiteral("int8"), QStringLiteral("int16"), QStringLiteral("int32"),
                QStringLiteral("int64"), QStringLiteral("uint"), QStringLiteral("float32"), QStringLiteral("float64"),
                QStringLiteral("bool"), QStringLiteral("byte"), QStringLiteral("rune"), QStringLiteral("string")
            }, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

void registerPhpLanguage()
{
    LanguageRegistry::instance().registerLanguage({
        QStringLiteral("php"),
        QStringLiteral("PHP"),
        {QStringLiteral("php")},
        {},
        QStringLiteral("//"),
        [](QTextDocument* document) -> QStyleSyntaxHighlighter* {
            return createRulesHighlighter({
                QStringLiteral("class"), QStringLiteral("function"), QStringLiteral("public"), QStringLiteral("private"),
                QStringLiteral("protected"), QStringLiteral("if"), QStringLiteral("else"), QStringLiteral("elseif"),
                QStringLiteral("return"), QStringLiteral("foreach"), QStringLiteral("while"), QStringLiteral("namespace"),
                QStringLiteral("use"), QStringLiteral("extends"), QStringLiteral("implements"), QStringLiteral("trait"),
                QStringLiteral("static"), QStringLiteral("new")
            }, {
                QStringLiteral("int"), QStringLiteral("float"), QStringLiteral("bool"), QStringLiteral("string"),
                QStringLiteral("array"), QStringLiteral("void")
            }, QRegularExpression(QStringLiteral("//[^\\n]*")), document);
        }
    });
}

} // namespace

CREMNIY_REGISTER_LANGUAGE(registerJavaLanguage)
CREMNIY_REGISTER_LANGUAGE(registerCSharpLanguage)
CREMNIY_REGISTER_LANGUAGE(registerGoLanguage)
CREMNIY_REGISTER_LANGUAGE(registerPhpLanguage)
