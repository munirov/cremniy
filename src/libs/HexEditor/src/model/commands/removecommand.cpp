#include <QHexView/model/commands/removecommand.h>
#include <QHexView/model/qhexdocument.h>

QHexViewRemoveCommand::QHexViewRemoveCommand(QHexBuffer* buffer,
                                             const QHexChanges& changes,
                                             QHexDocument* document,
                                             qint64 offset, int length,
                                             QUndoCommand* parent)
    : QHexViewCommand(buffer, changes, document, parent) {
    m_offset = offset;
    m_length = length;
}

void QHexViewRemoveCommand::undo() {
    m_buffer->insert(m_offset, m_data);
    Q_EMIT m_hexdocument->dataChanged(m_data, m_offset,
                                      QHexChangeReason::Insert);
}

void QHexViewRemoveCommand::redo() {
    m_data = m_buffer->read(m_offset, m_length); // Backup data
    m_buffer->remove(m_offset, m_length);
}
