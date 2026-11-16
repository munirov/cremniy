#include "widgets/EditorLanguageSupport.h"

#include "languages/LanguageRegistry.h"

namespace EditorLanguageSupport {

QString normalizedFileExt(const QString& ext)
{
    const QString value = ext.trimmed().toLower();
    const LanguageDefinition& lang = LanguageRegistry::instance().resolveForExtension(value);
    if (&lang == &LanguageRegistry::plainTextLanguage() && lang.id != value)
        return value; // Unknown extension: preserve the raw (lowercased) value, as before.
    return lang.id;
}

QString syntaxKeyForFileName(const QString& fileName)
{
    return LanguageRegistry::instance().resolveForFile(fileName).id;
}

QString lineCommentPrefix(const QString& syntaxKey)
{
    if (const LanguageDefinition* lang = LanguageRegistry::instance().find(syntaxKey))
        return lang->lineCommentPrefix;
    return QStringLiteral("//");
}

} // namespace EditorLanguageSupport
