#ifndef LANGUAGEREGISTRY_H
#define LANGUAGEREGISTRY_H

#include "languages/LanguageDefinition.h"

#include <QHash>
#include <QString>
#include <QVector>

class QStyleSyntaxHighlighter;
class QTextDocument;

/**
 * @brief Центральный реестр всех поддерживаемых языков редактора кода.
 *
 * Это единственное место, которое CustomCodeEditor опрашивает, чтобы:
 *   - определить язык файла по расширению или имени файла;
 *   - получить префикс комментария для этого языка;
 *   - создать хайлайтер для документа.
 *
 * Языки НЕ регистрируются вручную в этом файле или в CustomCodeEditor.cpp.
 * Каждый язык живёт в своей директории src/languages/<name>/ и регистрирует себя
 * сам при старте программы через макрос CREMNIY_REGISTER_LANGUAGE (ниже) —
 * то есть добавление языка сводится к добавлению одного нового файла.
 *
 * Подробное руководство: docs/adding_a_language.md (docs/adding_a_language_ru.md).
 */
class LanguageRegistry {
public:
    static LanguageRegistry& instance();

    /// Регистрирует язык. Повторная регистрация одного и того же id
    /// перезаписывает предыдущее определение (полезно для тестов/патчей),
    /// но в обычном коде каждый id должен регистрироваться ровно один раз.
    void registerLanguage(LanguageDefinition definition);

    /// Возвращает определение языка по его id, либо nullptr, если такого
    /// языка нет в реестре.
    const LanguageDefinition* find(const QString& id) const;

    /// Определяет язык по расширению файла (без точки, регистр неважен)
    /// или по точному имени файла. exactFileNames проверяются раньше
    /// расширения. Если совпадений нет — возвращает plainTextLanguage()
    /// (redirect на язык "text", который не подсвечивается).
    const LanguageDefinition& resolveForFile(const QString& fileName) const;

    /// То же самое, но когда на входе уже есть "нормализованное" расширение
    /// (используется для обратной совместимости с существующим кодом,
    /// который оперирует расширениями напрямую).
    const LanguageDefinition& resolveForExtension(const QString& extension) const;

    /// Список всех зарегистрированных языков, отсортированный по displayName.
    /// Полезен для UI (например, меню "Set language...").
    QVector<const LanguageDefinition*> allLanguages() const;

    /// Определение "языка по умолчанию" для неизвестных/бинарных
    /// расширений — обычный текст без подсветки.
    static const LanguageDefinition& plainTextLanguage();

private:
    LanguageRegistry() = default;
    QHash<QString, LanguageDefinition> m_byId;
    QHash<QString, QString> m_idByExtension;
    QHash<QString, QString> m_idByExactFileName;
};

/// Регистрирует функцию, которая будет вызвана один раз при старте программы
/// и должна вызвать LanguageRegistry::instance().registerLanguage(...).
///
/// Пример использования — в конце файла src/languages/python/PythonLanguage.cpp:
///
///     CREMNIY_REGISTER_LANGUAGE(registerPythonLanguage)
///
/// где registerPythonLanguage — функция void(), объявленная в этом же файле.
/// См. docs/adding_a_language.md для полного примера.
///
/// Реализация НЕ полагается только на порядок статической инициализации:
/// поскольку CodeEditor собирается как статическая библиотека, линкер волен
/// отбросить .o-файл языка целиком, если на него нет ни одной явной ссылки
/// (побочный эффект конструктора статического объекта для линкера "ссылкой"
/// не считается). Поэтому макрос дополнительно объявляет функцию с внешним
/// связыванием (`<имя>_forceLink`), на которую src/languages/core/LanguageRegistration.cpp
/// ссылается явно, а main() вызывает registerAllLanguages() один раз при
/// старте программы, до создания первого CustomCodeEditor — это гарантирует,
/// что object file языка попадёт в финальный бинарник независимо от
/// поведения линкера.
///
/// При добавлении нового языка: помимо CREMNIY_REGISTER_LANGUAGE в новом
/// .cpp-файле нужно добавить одну строку (extern-объявление + вызов) в
/// src/languages/core/LanguageRegistration.cpp — см. комментарий в том файле
/// и docs/adding_a_language.md.
#define CREMNIY_REGISTER_LANGUAGE(registerFn)                                 \
    namespace {                                                              \
    struct RegisterFn##registerFn##_ {                                       \
        RegisterFn##registerFn##_() { registerFn(); }                       \
    };                                                                       \
    static RegisterFn##registerFn##_ registerFn##_instance_;                 \
    }                                                                        \
    void registerFn##_forceLink() { registerFn(); }

#endif // LANGUAGEREGISTRY_H
