#ifndef EDITORTYPINGBEHAVIORS_H
#define EDITORTYPINGBEHAVIORS_H

#include <QByteArray>
#include <QChar>
#include <QString>
#include <QStringList>

#include <optional>

namespace EditorTypingBehaviors {

struct IndentDecision {
    bool shouldIncrease = false;
    QChar splitCloser;
};

QString indentUnitText(bool tabReplace, int tabReplaceSize);
IndentDecision indentationAfterPrefix(const QString& trimmedPrefix);
std::optional<QByteArray> emptyLineEnterReplacement(const QString& lineText,
                                                    const QString& indent,
                                                    const QString& indentUnit,
                                                    bool tabReplace);
int smartBackspaceColumnDeleteCount(const QString& linePrefix, int tabReplaceSize, bool tabReplace);
bool isSymmetricPairChar(QChar ch);
bool isMatchingPair(QChar opening, QChar closing);
QByteArray matchingPair(const QString& text);
bool shouldSkipClosing(const QString& text, char byteUnderCursor);
std::optional<QString> autoDedentedPrefix(const QString& text,
                                          const QString& linePrefix,
                                          const QString& lineSuffix,
                                          const QString& indentUnit,
                                          bool tabReplace);
QStringList toggleLineComments(const QStringList& lines, const QString& commentPrefix);
QString smartPasteText(const QString& text, const QString& currentLineIndent);

} // namespace EditorTypingBehaviors

#endif // EDITORTYPINGBEHAVIORS_H
