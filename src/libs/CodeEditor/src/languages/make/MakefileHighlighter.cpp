#include "MakefileHighlighter.h"

#include "QSyntaxStyle.hpp"

#include <QRegularExpression>

MakefileHighlighter::MakefileHighlighter(QTextDocument* document)
    : QStyleSyntaxHighlighter(document)
{
}

void MakefileHighlighter::highlightBlock(const QString& text)
{
    static const QRegularExpression commentPattern(QStringLiteral("#[^\\n]*"));
    static const QRegularExpression stringPattern(QStringLiteral("\"[^\"\\n]*\"|'[^'\\n]*'"));
    static const QRegularExpression variablePattern(QStringLiteral("\\$\\((?:[^()\\\\]|\\\\.)+\\)|\\$\\{(?:[^{}\\\\]|\\\\.)+\\}|\\$[@%<?^+*|]"));
    static const QRegularExpression directivePattern(QStringLiteral("^\\s*(include|ifdef|ifndef|else|endif|ifeq|ifneq|define|endef|override|export|unexport)\\b"));
    static const QRegularExpression assignmentPattern(QStringLiteral("^[^:#=\\s][^:=#]*\\s*(?:[:+?]?=)"));
    static const QRegularExpression targetPattern(QStringLiteral("^(?:\\.[A-Z_][A-Z0-9_]*|[^:#=\\s][^:=#]*)\\s*:"));
    static const QRegularExpression functionPattern(QStringLiteral("\\$\\((subst|patsubst|strip|findstring|filter|filter-out|sort|word|wordlist|words|firstword|lastword|dir|notdir|suffix|basename|addsuffix|addprefix|join|foreach|if|or|and|call|value|eval|file|shell|wildcard|error|warn|info)\\b"));

    auto applyAll = [this, &text](const QRegularExpression& pattern, const QString& styleName, int captureGroup = 0) {
        auto matches = pattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            const int start = captureGroup == 0 ? match.capturedStart() : match.capturedStart(captureGroup);
            const int length = captureGroup == 0 ? match.capturedLength() : match.capturedLength(captureGroup);
            if (start >= 0 && length > 0)
                setFormat(start, length, syntaxStyle()->getFormat(styleName));
        }
    };

    applyAll(stringPattern, QStringLiteral("String"));
    applyAll(variablePattern, QStringLiteral("PrimitiveType"));
    applyAll(directivePattern, QStringLiteral("Keyword"), 1);
    applyAll(functionPattern, QStringLiteral("Function"), 1);

    const auto assignmentMatch = assignmentPattern.match(text);
    if (assignmentMatch.hasMatch())
        setFormat(assignmentMatch.capturedStart(), assignmentMatch.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Function")));

    const auto targetMatch = targetPattern.match(text);
    if (targetMatch.hasMatch())
        setFormat(targetMatch.capturedStart(), targetMatch.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Keyword")));

    applyAll(commentPattern, QStringLiteral("Comment"));
}
