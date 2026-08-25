#pragma once

#include "QHighlightBlockRule.hpp"
#include "QHighlightRule.hpp"
#include "QStyleSyntaxHighlighter.hpp"

#include <QRegularExpression>
#include <QVector>

class AsmHighlighter : public QStyleSyntaxHighlighter {
public:
    explicit AsmHighlighter(QTextDocument* document = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    QVector<QHighlightRule> m_rules;
    QVector<QHighlightBlockRule> m_blockRules;
    QRegularExpression m_commentPattern;
    QRegularExpression m_numberPattern;
    QRegularExpression m_stringPattern;
    QRegularExpression m_labelPattern;
};
