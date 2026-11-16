#include "xml/XmlLanguageHighlighter.h"

#include "QLanguage.hpp"
#include "QSyntaxStyle.hpp"

#include <QFile>

XmlLanguageHighlighter::XmlLanguageHighlighter(const QString& resourcePath,
                                               const QRegularExpression& singleLineComment,
                                               const QRegularExpression& stringPattern,
                                               const QRegularExpression& numberPattern,
                                               QTextDocument* document)
    : QStyleSyntaxHighlighter(document)
    , m_singleLineComment(singleLineComment)
    , m_stringPattern(stringPattern)
    , m_numberPattern(numberPattern)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QLanguage language(&file);
    if (!language.isLoaded())
        return;

    for (const QString& key : language.keys()) {
        const QString formatName = key == "Directive" || key == "Command" || key == "Variable" || key == "BuiltinFunction"
                                       ? QStringLiteral("Keyword")
                                       : key;
        for (const QString& name : language.names(key)) {
            const QString escaped = QRegularExpression::escape(name);
            m_rules.append({QRegularExpression(QStringLiteral("\\b%1\\b").arg(escaped)), formatName});
        }
    }
}

void XmlLanguageHighlighter::highlightBlock(const QString& text)
{
    if (m_numberPattern.isValid()) {
        auto matches = m_numberPattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Number")));
        }
    }

    if (m_stringPattern.isValid()) {
        auto matches = m_stringPattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("String")));
        }
    }

    for (const auto& rule : m_rules) {
        auto matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(rule.formatName));
        }
    }

    if (m_singleLineComment.isValid()) {
        auto matches = m_singleLineComment.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Comment")));
        }
    }
}
