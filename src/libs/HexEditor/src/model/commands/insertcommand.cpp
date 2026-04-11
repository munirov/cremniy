#include <QHexView/model/commands/insertcommand.h>
#include <QHexView/model/qhexdocument.h>

QHexViewInsertCommand::QHexViewInsertCommand(
    QHexBuffer* buffer, const QHexChanges& changes, QHexDocument* document,
    qint64 offset, const QByteArray& data, QUndoCommand* parent)
    : QHexViewCommand(buffer, changes, document, parent) {
    m_offset = offset;
    m_data = data;
}

void QHexViewInsertCommand::undo() {
    m_buffer->remove(m_offset, m_data.length());
    Q_EMIT m_hexdocument->dataChanged(m_data, m_offset,
                                      QHexChangeReason::Remove);
}

void QHexViewInsertCommand::redo() { m_buffer->insert(m_offset, m_data); }
