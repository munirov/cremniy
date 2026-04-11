#ifndef EDITORLANGUAGESUPPORT_H
#define EDITORLANGUAGESUPPORT_H

#include <QString>

namespace EditorLanguageSupport {

QString normalizedFileExt(const QString& ext);
QString lineCommentPrefix(const QString& syntaxKey);
QString languageResourceForExtension(const QString& ext);
bool isCommonRuleLanguageResource(const QString& resource);

} // namespace EditorLanguageSupport

#endif // EDITORLANGUAGESUPPORT_H
