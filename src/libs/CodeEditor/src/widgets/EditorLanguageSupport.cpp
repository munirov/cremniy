#include "widgets/EditorLanguageSupport.h"

#include <QHash>

namespace EditorLanguageSupport {

QString normalizedFileExt(const QString& ext)
{
    QString value = ext.trimmed().toLower();
    if (value == QStringLiteral("makefile"))
        return QStringLiteral("make");
    if (value == QStringLiteral("cmakelists.txt"))
        return QStringLiteral("cmake");
    if (value == QStringLiteral("yml"))
        return QStringLiteral("yaml");
    if (value == QStringLiteral("bash") || value == QStringLiteral("zsh") || value == QStringLiteral("fish"))
        return QStringLiteral("sh");
    if (value == QStringLiteral("mjs") || value == QStringLiteral("cjs") || value == QStringLiteral("jsx"))
        return QStringLiteral("js");
    if (value == QStringLiteral("tsx"))
        return QStringLiteral("ts");
    if (value == QStringLiteral("cfg") || value == QStringLiteral("conf") || value == QStringLiteral("properties") || value == QStringLiteral("env"))
        return QStringLiteral("ini");
    return value;
}

QString lineCommentPrefix(const QString& syntaxKey)
{
    if (syntaxKey == QStringLiteral("py") || syntaxKey == QStringLiteral("yaml") || syntaxKey == QStringLiteral("toml") ||
        syntaxKey == QStringLiteral("ini") || syntaxKey == QStringLiteral("sh") || syntaxKey == QStringLiteral("make") ||
        syntaxKey == QStringLiteral("cmake"))
        return QStringLiteral("#");
    if (syntaxKey == QStringLiteral("sql"))
        return QStringLiteral("--");
    if (syntaxKey == QStringLiteral("xml"))
        return {};
    return QStringLiteral("//");
}

QString languageResourceForExtension(const QString& ext)
{
    static const QHash<QString, QString> resources = {
        {QStringLiteral("c"), QStringLiteral(":/languages/c.xml")},
        {QStringLiteral("h"), QStringLiteral(":/languages/c.xml")},
        {QStringLiteral("cpp"), QStringLiteral(":/languages/cpp.xml")},
        {QStringLiteral("cc"), QStringLiteral(":/languages/cpp.xml")},
        {QStringLiteral("cxx"), QStringLiteral(":/languages/cpp.xml")},
        {QStringLiteral("hpp"), QStringLiteral(":/languages/cpp.xml")},
        {QStringLiteral("hh"), QStringLiteral(":/languages/cpp.xml")},
        {QStringLiteral("hxx"), QStringLiteral(":/languages/cpp.xml")},
        {QStringLiteral("asm"), QStringLiteral(":/languages/asm.xml")},
        {QStringLiteral("s"), QStringLiteral(":/languages/asm.xml")},
        {QStringLiteral("py"), QStringLiteral(":/languages/python.xml")},
        {QStringLiteral("lua"), QStringLiteral(":/languages/lua.xml")},
        {QStringLiteral("make"), QStringLiteral(":/languages/gnumake.xml")},
        {QStringLiteral("cmake"), QStringLiteral(":/languages/cmake.xml")},
        {QStringLiteral("md"), QStringLiteral(":/languages/markdown")},
        {QStringLiteral("markdown"), QStringLiteral(":/languages/markdown")},
        {QStringLiteral("json"), QStringLiteral(":/languages/json")},
        {QStringLiteral("yaml"), QStringLiteral(":/languages/yaml")},
        {QStringLiteral("toml"), QStringLiteral(":/languages/toml")},
        {QStringLiteral("ini"), QStringLiteral(":/languages/ini")},
        {QStringLiteral("sh"), QStringLiteral(":/languages/sh")},
        {QStringLiteral("js"), QStringLiteral(":/languages/js")},
        {QStringLiteral("ts"), QStringLiteral(":/languages/ts")},
        {QStringLiteral("java"), QStringLiteral(":/languages/java")},
        {QStringLiteral("cs"), QStringLiteral(":/languages/cs")},
        {QStringLiteral("go"), QStringLiteral(":/languages/go")},
        {QStringLiteral("php"), QStringLiteral(":/languages/php")},
        {QStringLiteral("sql"), QStringLiteral(":/languages/sql")},
        {QStringLiteral("xml"), QStringLiteral(":/languages/xml")},
        {QStringLiteral("xaml"), QStringLiteral(":/languages/xml")},
        {QStringLiteral("svg"), QStringLiteral(":/languages/xml")},
        {QStringLiteral("sln"), QStringLiteral(":/languages/sln")}
    };

    return resources.value(normalizedFileExt(ext), QStringLiteral(":/languages/cpp.xml"));
}

bool isCommonRuleLanguageResource(const QString& resource)
{
    return resource == QStringLiteral(":/languages/yaml")
        || resource == QStringLiteral(":/languages/toml")
        || resource == QStringLiteral(":/languages/ini")
        || resource == QStringLiteral(":/languages/sh")
        || resource == QStringLiteral(":/languages/js")
        || resource == QStringLiteral(":/languages/ts")
        || resource == QStringLiteral(":/languages/java")
        || resource == QStringLiteral(":/languages/cs")
        || resource == QStringLiteral(":/languages/go")
        || resource == QStringLiteral(":/languages/php")
        || resource == QStringLiteral(":/languages/sql")
        || resource == QStringLiteral(":/languages/xml")
        || resource == QStringLiteral(":/languages/sln");
}

} // namespace EditorLanguageSupport
