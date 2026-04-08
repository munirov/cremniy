#include <QHexView/model/commands/replacecommand.h>
#include <QHexView/model/qhexdocument.h>

QHexViewReplaceCommand::QHexViewReplaceCommand(
    QHexBuffer* buffer, const QHexChanges& changes, QHexDocument* document,
    qint64 offset, const QByteArray& data, QUndoCommand* parent)
    : QHexViewCommand(buffer, changes, document, parent) {
    m_offset = offset;
    m_data = data;
}

void QHexViewReplaceCommand::undo() {
    m_buffer->replace(m_offset, m_olddata);
    Q_EMIT m_hexdocument->dataChanged(m_olddata, m_offset,
                                      QHexChangeReason::Replace);
}

void QHexViewReplaceCommand::redo() {
    m_olddata = m_buffer->read(m_offset, m_data.length());
    m_buffer->replace(m_offset, m_data);
}
