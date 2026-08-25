#include "core/RuleBasedHighlighter.h"

#include "QSyntaxStyle.hpp"

RuleBasedHighlighter::RuleBasedHighlighter(QVector<RegexRule> rules,
                                           const QRegularExpression& commentPattern,
                                           QTextDocument* document)
    : QStyleSyntaxHighlighter(document)
    , m_rules(rules)
    , m_commentPattern(commentPattern)
{
}

void RuleBasedHighlighter::highlightBlock(const QString& text)
{
    for (const auto& rule : m_rules) {
        auto matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(rule.formatName));
        }
    }

    if (m_commentPattern.isValid()) {
        auto matches = m_commentPattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Comment")));
        }
    }
}
