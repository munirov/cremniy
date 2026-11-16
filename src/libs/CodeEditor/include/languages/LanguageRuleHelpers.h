#ifndef LANGUAGERULEHELPERS_H
#define LANGUAGERULEHELPERS_H

#include "core/RuleBasedHighlighter.h"

#include <QRegularExpression>
#include <QStringList>
#include <QVector>

/**
 * @brief Общие хелперы для построения RegexRule-наборов простых языков.
 *
 * Большинству новых языков не нужен собственный C++-класс хайлайтера —
 * достаточно набора regex-правил (ключевые слова, строки, числа, комментарий),
 * переданных в RuleBasedHighlighter. Эти хелперы избавляют от копипасты
 * при написании таких правил. Смотри существующие файлы в src/languages/
 * как примеры использования.
 */
namespace LanguageRuleHelpers {

/// Строит набор правил "слово в списке -> формат" (например ключевые слова).
/// Каждое слово оборачивается в \b...\b, спецсимволы экранируются.
inline QVector<RegexRule> wordRules(const QStringList& words, const QString& formatName)
{
    QVector<RegexRule> rules;
    rules.reserve(words.size());
    for (const QString& word : words)
        rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(word))),
                       formatName});
    return rules;
}

/// Стандартное правило для строковых литералов в двойных/одинарных кавычках
/// (без экранирования кавычек внутри — этого достаточно для подсветки).
inline RegexRule stringLiteralRule()
{
    return {QRegularExpression(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'")), QStringLiteral("String")};
}

/// Стандартное правило для числовых литералов (целые, дробные, hex).
inline RegexRule numberLiteralRule()
{
    return {QRegularExpression(QStringLiteral("\\b(0x[0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")), QStringLiteral("Number")};
}

/// Набор из строк + чисел — то, что нужно почти каждому языку.
inline QVector<RegexRule> literalRules()
{
    return {stringLiteralRule(), numberLiteralRule()};
}

/// Готовый набор правил для "C-подобных" языков с раздельными списками
/// ключевых слов и примитивных типов поверх обычных строк/чисел.
/// Используется семьёй языков с фигурными скобками (Java, C#, Go, PHP) —
/// каждый передаёт свои списки слов, но форма правил одинакова.
inline QVector<RegexRule> keywordsAndTypesRules(const QStringList& keywords, const QStringList& primitiveTypes)
{
    QVector<RegexRule> rules = literalRules();
    rules += wordRules(keywords, QStringLiteral("Keyword"));
    rules += wordRules(primitiveTypes, QStringLiteral("PrimitiveType"));
    return rules;
}

} // namespace LanguageRuleHelpers

#endif // LANGUAGERULEHELPERS_H
