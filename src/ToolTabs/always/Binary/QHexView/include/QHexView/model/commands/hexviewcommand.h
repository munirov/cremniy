#pragma once

#include <QHexView/model/buffer/qhexbuffer.h>
#include <QHexView/model/qhexchanges.h>
#include <QUndoCommand>

class QHexDocument;

class QHexViewCommand: public QUndoCommand {
public:
    QHexViewCommand(QHexBuffer* buffer, const QHexChanges& changes,
                    QHexDocument* document, QUndoCommand* parent = nullptr);
    const QHexChanges& changes() const { return m_changes; }

protected:
    QHexChanges m_changes;
    QHexDocument* m_hexdocument;
    QHexBuffer* m_buffer;
    qint64 m_offset;
    int m_length;
    QByteArray m_data;
};
