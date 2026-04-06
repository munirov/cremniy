#include "widgets/EditorTypingBehaviors.h"

#include <QLatin1Char>
#include <QRegularExpression>

namespace EditorTypingBehaviors {

QString indentUnitText(bool tabReplace, int tabReplaceSize)
{
    return tabReplace ? QString(tabReplaceSize, QLatin1Char(' '))
                      : QString(QLatin1Char('\t'));
}

IndentDecision indentationAfterPrefix(const QString& trimmedPrefix)
{
    const QChar opener = trimmedPrefix.isEmpty() ? QChar() : trimmedPrefix.back();

    IndentDecision decision;
    decision.shouldIncrease = opener == QLatin1Char('{')
        || opener == QLatin1Char('(')
        || opener == QLatin1Char('[')
        || opener == QLatin1Char(':');
    decision.splitCloser = opener == QLatin1Char('{') ? QLatin1Char('}')
        : opener == QLatin1Char('(') ? QLatin1Char(')')
        : opener == QLatin1Char('[') ? QLatin1Char(']')
        : QChar();
    return decision;
}

std::optional<QByteArray> emptyLineEnterReplacement(const QString& lineText,
                                                    const QString& indent,
                                                    const QString& indentUnit,
                                                    bool tabReplace)
{
    if (lineText.isEmpty() || !lineText.trimmed().isEmpty())
        return std::nullopt;

    QString nextIndent = indent;
    if (!nextIndent.isEmpty()) {
        if (nextIndent.endsWith(indentUnit))
            nextIndent.chop(indentUnit.size());
        else if (!tabReplace && nextIndent.endsWith(QLatin1Char('\t')))
            nextIndent.chop(1);
    }

    QByteArray replacement("\n");
    replacement.append(nextIndent.toUtf8());
    return replacement;
}

int smartBackspaceColumnDeleteCount(const QString& linePrefix, int tabReplaceSize, bool tabReplace)
{
    if (linePrefix.isEmpty() || !linePrefix.trimmed().isEmpty())
        return 0;

    if (!tabReplace)
        return linePrefix.endsWith(QLatin1Char('\t')) ? 1 : 0;

    int trailingSpaces = 0;
    for (int i = linePrefix.size() - 1; i >= 0 && linePrefix.at(i) == QLatin1Char(' '); --i)
        ++trailingSpaces;

    if (trailingSpaces == 0)
        return 0;

    const int remainder = trailingSpaces % tabReplaceSize;
    return remainder == 0 ? qMin(tabReplaceSize, trailingSpaces) : remainder;
}

bool isSymmetricPairChar(QChar ch)
{
    return ch == QLatin1Char('"') || ch == QLatin1Char('\'');
}

bool isMatchingPair(QChar opening, QChar closing)
{
    return (opening == QLatin1Char('(') && closing == QLatin1Char(')'))
        || (opening == QLatin1Char('[') && closing == QLatin1Char(']'))
        || (opening == QLatin1Char('{') && closing == QLatin1Char('}'))
        || (isSymmetricPairChar(opening) && opening == closing);
}

QByteArray matchingPair(const QString& text)
{
    if (text == QStringLiteral("("))
        return QByteArray("()");
    if (text == QStringLiteral("["))
        return QByteArray("[]");
    if (text == QStringLiteral("{"))
        return QByteArray("{}");
    if (text == QStringLiteral("\""))
        return QByteArray("\"\"");
    if (text == QStringLiteral("'"))
        return QByteArray("''");
    return QByteArray();
}

bool shouldSkipClosing(const QString& text, char byteUnderCursor)
{
    return (text == QStringLiteral(")") && byteUnderCursor == ')')
        || (text == QStringLiteral("]") && byteUnderCursor == ']')
        || (text == QStringLiteral("}") && byteUnderCursor == '}')
        || (text == QStringLiteral("\"") && byteUnderCursor == '"')
        || (text == QStringLiteral("'") && byteUnderCursor == '\'');
}

std::optional<QString> autoDedentedPrefix(const QString& text,
                                          const QString& linePrefix,
                                          const QString& lineSuffix,
                                          const QString& indentUnit,
                                          bool tabReplace)
{
    if (text.size() != 1)
        return std::nullopt;

    const QChar closing = text.front();
    if (closing != QLatin1Char('}') && closing != QLatin1Char(')') && closing != QLatin1Char(']'))
        return std::nullopt;

    if (!linePrefix.trimmed().isEmpty() || !lineSuffix.trimmed().isEmpty())
        return std::nullopt;

    QString dedentedPrefix = linePrefix;
    if (dedentedPrefix.endsWith(indentUnit))
        dedentedPrefix.chop(indentUnit.size());
    else if (!tabReplace && dedentedPrefix.endsWith(QLatin1Char('\t')))
        dedentedPrefix.chop(1);

    if (dedentedPrefix == linePrefix)
        return std::nullopt;

    return dedentedPrefix;
}

QStringList toggleLineComments(const QStringList& lines, const QString& commentPrefix)
{
    if (commentPrefix.isEmpty())
        return lines;

    bool canUncomment = true;
    for (const QString& text : lines) {
        const QString trimmed = text.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith(commentPrefix)) {
            canUncomment = false;
            break;
        }
    }

    QStringList updatedLines;
    updatedLines.reserve(lines.size());

    for (const QString& text : lines) {
        QString updated = text;
        if (canUncomment) {
            const int indent = text.indexOf(QRegularExpression(QStringLiteral("\\S")));
            if (indent >= 0 && text.mid(indent).startsWith(commentPrefix)) {
                updated.remove(indent, commentPrefix.length());
                if (updated.mid(indent).startsWith(QLatin1Char(' ')))
                    updated.remove(indent, 1);
            }
        } else {
            const QString trimmed = text.trimmed();
            const int indent = text.indexOf(QRegularExpression(QStringLiteral("\\S")));
            const int insertPos = indent >= 0 ? indent : text.length();
            updated.insert(insertPos, trimmed.isEmpty() ? commentPrefix : commentPrefix + QLatin1Char(' '));
        }

        updatedLines.append(updated);
    }

    return updatedLines;
}

QString smartPasteText(const QString& text, const QString& currentLineIndent)
{
    if (text.isEmpty() || !text.contains(QLatin1Char('\n')))
        return text;

    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));

    QStringList lines = normalized.split(QLatin1Char('\n'));
    for (int i = 1; i < lines.size(); ++i) {
        if (!lines.at(i).isEmpty())
            lines[i].prepend(currentLineIndent);
    }

    return lines.join(QLatin1Char('\n'));
}

} // namespace EditorTypingBehaviors
