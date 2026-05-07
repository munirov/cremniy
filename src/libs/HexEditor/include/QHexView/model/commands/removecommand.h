#pragma once

#include <QHexView/model/commands/hexviewcommand.h>

class QHexViewRemoveCommand: public QHexViewCommand {
public:
    QHexViewRemoveCommand(QHexBuffer* buffer, const QHexChanges& changes,
                          QHexDocument* document, qint64 offset, int length,
                          QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
};
