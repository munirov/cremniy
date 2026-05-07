#pragma once

#include <QHexView/model/commands/hexviewcommand.h>

class QHexViewReplaceCommand: public QHexViewCommand {
public:
    QHexViewReplaceCommand(QHexBuffer* buffer, const QHexChanges& changes,
                           QHexDocument* document, qint64 offset,
                           const QByteArray& data,
                           QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    QByteArray m_olddata;
};
