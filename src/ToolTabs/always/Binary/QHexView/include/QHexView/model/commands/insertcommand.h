#pragma once

#include <QHexView/model/commands/hexviewcommand.h>

class QHexViewInsertCommand: public QHexViewCommand {
public:
    QHexViewInsertCommand(QHexBuffer* buffer, const QHexChanges& changes,
                          QHexDocument* document, qint64 offset,
                          const QByteArray& data,
                          QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
};
