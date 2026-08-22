#ifndef LANGUAGEDEFINITION_H
#define LANGUAGEDEFINITION_H

#include <functional>

#include <QString>
#include <QStringList>

class QStyleSyntaxHighlighter;
class QTextDocument;

/**
 * @brief Единое описание поддержки языка в редакторе кода.
 *
 * Каждый язык (или семейство файлов: конфиги, разметка и т.п.) описывается
 * ровно одним LanguageDefinition. Это единственная точка, которую нужно
 * заполнить, чтобы редактор узнал новый язык: расширения файлов, по которым
 * он определяется, префикс однострочного комментария и фабрику, создающую
 * QStyleSyntaxHighlighter для документа.
 *
 * LanguageDefinition не создаётся вручную конечными потребителями —
 * он регистрируется в LanguageRegistry через LanguageRegistry::registerLanguage()
 * или макрос CREMNIY_REGISTER_LANGUAGE (см. LanguageRegistry.h).
 */
struct LanguageDefinition {
    /// Уникальный ключ языка, например "python", "rust", "cpp".
    /// Используется как syntaxKey редактора и как основной ID везде,
    /// где раньше использовалось "расширение файла".
    QString id;

    /// Человекочитаемое имя для UI (например, в статус-баре или меню "Language").
    QString displayName;

    /// Расширения файлов без точки, в нижнем регистре, которые относятся
    /// к этому языку, например {"py"} или {"yml", "yaml"}.
    /// Первое расширение в списке считается "каноническим".
    QStringList fileExtensions;

    /// Точные имена файлов (без пути), которые нужно распознавать отдельно
    /// от расширения, например {"Makefile", "CMakeLists.txt", "Dockerfile"}.
    /// Сравнение регистронезависимое (в отличие от файловой системы Linux) —
    /// это осознанный компромисс, чтобы не перечислять все варианты
    /// написания вроде "makefile"/"Makefile"/"MAKEFILE".
    QStringList exactFileNames;

    /// Префикс однострочного комментария, используемый функцией
    /// "закомментировать строку" (Ctrl+/). Пустая строка — язык не имеет
    /// однострочных комментариев (или закомментирование недоступно).
    QString lineCommentPrefix = QStringLiteral("//");

    /// Фабрика хайлайтера. Обязана быть валидной (не nullptr), за исключением
    /// языков, которые сознательно не подсвечиваются (см. plainTextLanguage()
    /// в LanguageRegistry) — тогда factory может вернуть nullptr.
    /// document передаётся во владение QStyleSyntaxHighlighter (см. Qt docs).
    std::function<QStyleSyntaxHighlighter*(QTextDocument* document)> createHighlighter;
};

#endif // LANGUAGEDEFINITION_H
