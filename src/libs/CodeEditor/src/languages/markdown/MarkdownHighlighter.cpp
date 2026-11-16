#include "MarkdownHighlighter.h"

#include "QSyntaxStyle.hpp"

#include <QRegularExpression>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument* document)
    : QStyleSyntaxHighlighter(document)
{
}

void MarkdownHighlighter::highlightBlock(const QString& text)
{
    static const QRegularExpression headingPattern(QStringLiteral("^\\s*#{1,6}\\s.*$"));
    static const QRegularExpression listPattern(QStringLiteral("^\\s*([-*+]\\s|\\d+\\.\\s).*$"));
    static const QRegularExpression codeFencePattern(QStringLiteral("^\\s*```.*$"));
    static const QRegularExpression inlineCodePattern(QStringLiteral("`[^`]+`"));
    static const QRegularExpression emphasisPattern(QStringLiteral("(\\*\\*[^*]+\\*\\*|__[^_]+__|\\*[^*]+\\*|_[^_]+_)"));
    static const QRegularExpression linkPattern(QStringLiteral("\\[[^\\]]+\\]\\([^)]+\\)"));

    auto applyAll = [this, &text](const QRegularExpression& pattern, const QString& styleName) {
        auto matches = pattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(styleName));
        }
    };

    applyAll(headingPattern, QStringLiteral("Keyword"));
    applyAll(listPattern, QStringLiteral("Preprocessor"));
    applyAll(codeFencePattern, QStringLiteral("Comment"));
    applyAll(inlineCodePattern, QStringLiteral("String"));
    applyAll(emphasisPattern, QStringLiteral("Type"));
    applyAll(linkPattern, QStringLiteral("Function"));
}
