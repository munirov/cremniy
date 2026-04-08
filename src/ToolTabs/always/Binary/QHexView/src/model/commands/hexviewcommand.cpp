#include <QHexView/model/commands/hexviewcommand.h>

QHexViewCommand::QHexViewCommand(QHexBuffer* buffer, const QHexChanges& changes,
                                 QHexDocument* document, QUndoCommand* parent)
    : QUndoCommand(parent), m_changes{changes}, m_hexdocument{document},
      m_buffer{buffer}, m_offset{}, m_length{} {}
