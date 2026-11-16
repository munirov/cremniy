#pragma once

#include "QStyleSyntaxHighlighter.hpp"

#include <QRegularExpression>
#include <QVector>

struct RegexRule {
    QRegularExpression pattern;
    QString formatName;
};

class RuleBasedHighlighter : public QStyleSyntaxHighlighter {
public:
    RuleBasedHighlighter(QVector<RegexRule> rules,
                         const QRegularExpression& commentPattern,
                         QTextDocument* document = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    QVector<RegexRule> m_rules;
    QRegularExpression m_commentPattern;
};
