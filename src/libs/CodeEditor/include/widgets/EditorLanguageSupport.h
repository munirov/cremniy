#ifndef EDITORLANGUAGESUPPORT_H
#define EDITORLANGUAGESUPPORT_H

#include <QString>

/**
 * @brief Тонкая обёртка над LanguageRegistry для обратной совместимости.
 *
 * Раньше это пространство имён само хранило таблицы "расширение -> язык"
 * и "язык -> комментарий". Теперь единственный источник истины —
 * LanguageRegistry (см. include/languages/LanguageRegistry.h), а эти функции
 * лишь делегируют туда. Новый код должен обращаться к LanguageRegistry
 * напрямую; эти обёртки оставлены, чтобы не переписывать существующих
 * вызывающих (CustomCodeEditor, CodeEditorTab).
 */
namespace EditorLanguageSupport {

/// Нормализует "сырое" расширение/токен в id языка из LanguageRegistry.
/// Для расширений без зарегистрированного языка возвращает исходное
/// значение в нижнем регистре (используется, например, для показа
/// расширения в UI, даже если оно не поддерживается).
QString normalizedFileExt(const QString& ext);

/// Определяет язык по имени файла (включая точные имена вроде "Dockerfile")
/// и возвращает его id. Полная замена как старому syntaxKeyForPath, так и
/// комбинации suffix-эвристик, которая раньше была здесь же.
QString syntaxKeyForFileName(const QString& fileName);

/// Префикс однострочного комментария для языка с данным id.
QString lineCommentPrefix(const QString& syntaxKey);

} // namespace EditorLanguageSupport

#endif // EDITORLANGUAGESUPPORT_H
