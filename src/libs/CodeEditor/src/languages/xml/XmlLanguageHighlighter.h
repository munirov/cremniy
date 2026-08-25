#pragma once

#include "QStyleSyntaxHighlighter.hpp"

#include <QRegularExpression>
#include <QVector>

class XmlLanguageHighlighter : public QStyleSyntaxHighlighter {
public:
    XmlLanguageHighlighter(const QString& resourcePath,
                           const QRegularExpression& singleLineComment,
                           const QRegularExpression& stringPattern,
                           const QRegularExpression& numberPattern,
                           QTextDocument* document = nullptr);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QString formatName;
    };

    QVector<Rule> m_rules;
    QRegularExpression m_singleLineComment;
    QRegularExpression m_stringPattern;
    QRegularExpression m_numberPattern;
};
