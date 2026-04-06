#ifndef EDITOREDITOPERATIONS_H
#define EDITOREDITOPERATIONS_H

#include <QByteArray>

#include <optional>

namespace EditorEditOperations {

struct EditOperation {
    qint64 replaceStart = 0;
    qint64 replaceLength = 0;
    QByteArray replacement;
    qint64 selectionStart = 0;
    qint64 selectionLength = 0;
};

EditOperation duplicateSelection(qint64 selectionStart,
                                 qint64 selectionLength,
                                 const QByteArray& selectedBytes,
                                 bool duplicateAbove);

EditOperation duplicateLine(qint64 lineStart,
                            qint64 lineLength,
                            qint64 bufferSize,
                            const QByteArray& lineBytes,
                            const QByteArray& newlineBytes,
                            bool duplicateAbove);

std::optional<EditOperation> moveBlockUp(qint64 previousBlockStart,
                                         const QByteArray& previousBlockBytes,
                                         const QByteArray& movedBlockBytes);

std::optional<EditOperation> moveBlockDown(qint64 movedBlockStart,
                                           const QByteArray& movedBlockBytes,
                                           const QByteArray& nextBlockBytes);

} // namespace EditorEditOperations

#endif // EDITOREDITOPERATIONS_H
