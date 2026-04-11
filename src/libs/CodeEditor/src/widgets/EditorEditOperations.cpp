#include "widgets/EditorEditOperations.h"

namespace EditorEditOperations {

EditOperation duplicateSelection(qint64 selectionStart,
                                 qint64 selectionLength,
                                 const QByteArray& selectedBytes,
                                 bool duplicateAbove)
{
    EditOperation operation;
    operation.replaceStart = duplicateAbove ? selectionStart : (selectionStart + selectionLength);
    operation.replacement = selectedBytes;
    operation.selectionStart = operation.replaceStart;
    operation.selectionLength = selectedBytes.size();
    return operation;
}

EditOperation duplicateLine(qint64 lineStart,
                            qint64 lineLength,
                            qint64 bufferSize,
                            const QByteArray& lineBytes,
                            const QByteArray& newlineBytes,
                            bool duplicateAbove)
{
    EditOperation operation;
    operation.replacement = lineBytes + newlineBytes;
    operation.replaceStart = duplicateAbove ? lineStart : (lineStart + lineLength + (bufferSize > lineStart + lineLength ? newlineBytes.size() : 0));
    operation.selectionStart = operation.replaceStart;
    operation.selectionLength = lineBytes.size();
    return operation;
}

std::optional<EditOperation> moveBlockUp(qint64 previousBlockStart,
                                         const QByteArray& previousBlockBytes,
                                         const QByteArray& movedBlockBytes)
{
    if (previousBlockBytes.isEmpty() || movedBlockBytes.isEmpty())
        return std::nullopt;

    EditOperation operation;
    operation.replaceStart = previousBlockStart;
    operation.replaceLength = previousBlockBytes.size() + movedBlockBytes.size();
    operation.replacement = movedBlockBytes + previousBlockBytes;
    operation.selectionStart = previousBlockStart;
    operation.selectionLength = movedBlockBytes.size();
    return operation;
}

std::optional<EditOperation> moveBlockDown(qint64 movedBlockStart,
                                           const QByteArray& movedBlockBytes,
                                           const QByteArray& nextBlockBytes)
{
    if (movedBlockBytes.isEmpty() || nextBlockBytes.isEmpty())
        return std::nullopt;

    EditOperation operation;
    operation.replaceStart = movedBlockStart;
    operation.replaceLength = movedBlockBytes.size() + nextBlockBytes.size();
    operation.replacement = nextBlockBytes + movedBlockBytes;
    operation.selectionStart = movedBlockStart + nextBlockBytes.size();
    operation.selectionLength = movedBlockBytes.size();
    return operation;
}

} // namespace EditorEditOperations
