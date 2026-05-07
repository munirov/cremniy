#include <QBuffer>
#include <QFile>
#include <QHexView/model/buffer/qdevicebuffer.h>
#include <QHexView/model/buffer/qmappedfilebuffer.h>
#include <QHexView/model/buffer/qmemorybuffer.h>
#include <QHexView/model/commands/insertcommand.h>
#include <QHexView/model/commands/removecommand.h>
#include <QHexView/model/commands/replacecommand.h>
#include <QHexView/model/qhexdocument.h>
#include <cmath>

QHexDocument::QHexDocument(QHexBuffer* buffer, QObject* parent)
    : QObject(parent) {
    m_buffer = buffer;
    m_buffer->setParent(this); /* Take Ownership */

    m_undostack = new QUndoStack(this);

    connect(m_undostack, &QUndoStack::canUndoChanged, this,
            &QHexDocument::canUndoChanged);
    connect(m_undostack, &QUndoStack::canRedoChanged, this,
            &QHexDocument::canRedoChanged);
    connect(m_undostack, &QUndoStack::cleanChanged, this,
            [&](bool clean) { Q_EMIT modifiedChanged(!clean); });
}

qint64 QHexDocument::indexOf(const QByteArray& ba, qint64 from) {
    return m_buffer->indexOf(ba, from);
}

qint64 QHexDocument::lastIndexOf(const QByteArray& ba, qint64 from) {
    return m_buffer->lastIndexOf(ba, from);
}

QHexChangeReason QHexDocument::getChangeReason(qint64 offset) const {
    int idx = this->findChange(offset);
    return idx != -1 ? m_changes[idx].reason : QHexChangeReason::None;
}

qsizetype QHexDocument::findChange(qint64 offset) const {
    if(!m_trackchanges)
        return -1;

    int left = 0, right = m_changes.size() - 1;

    while(left <= right) {
        int mid = (left + right) / 2;
        const QHexChangeRange& r = m_changes[mid];

        if(offset < r.start)
            right = mid - 1;
        else if(offset >= r.end)
            left = mid + 1;
        else
            return mid; /* found */
    }

    return -1;
}

bool QHexDocument::accept(qint64 idx) const { return m_buffer->accept(idx); }
bool QHexDocument::isEmpty() const { return m_buffer->isEmpty(); }
bool QHexDocument::isModified() const { return !m_undostack->isClean(); }
bool QHexDocument::canUndo() const { return m_undostack->canUndo(); }
bool QHexDocument::canRedo() const { return m_undostack->canRedo(); }

bool QHexDocument::trackChanges() const { return m_trackchanges; }

void QHexDocument::setData(const QByteArray& ba) {
    QHexBuffer* mb = new QMemoryBuffer();
    mb->read(ba);
    this->setData(mb);
}

void QHexDocument::setData(QHexBuffer* buffer) {
    if(!buffer)
        return;

    m_changes.clear();
    m_undostack->clear();
    buffer->setParent(this);

    auto* oldbuffer = m_buffer;
    m_buffer = buffer;
    if(oldbuffer)
        oldbuffer->deleteLater();

    Q_EMIT canUndoChanged(false);
    Q_EMIT canRedoChanged(false);
    Q_EMIT changed();
    Q_EMIT reset();
}

void QHexDocument::setTrackChanges(bool b) {
    if(b == m_trackchanges)
        return;

    m_trackchanges = b;
    Q_EMIT trackChangesChanged(b);
}

void QHexDocument::clearChanges() {
    if(!m_trackchanges || m_changes.isEmpty())
        return;

    m_changes.clear();
    Q_EMIT changed();
}

void QHexDocument::clearModified() { m_undostack->setClean(); }

qint64 QHexDocument::length() const {
    return m_buffer ? m_buffer->length() : 0;
}

uchar QHexDocument::at(int offset) const { return m_buffer->at(offset); }

QHexDocument* QHexDocument::fromFile(QString filename, QObject* parent) {
    QFile f(filename);

    if(f.open(QFile::ReadOnly))
        return QHexDocument::fromMemory<QMemoryBuffer>(f.readAll(), parent);

    return nullptr;
}

void QHexDocument::undo() {
    m_undostack->undo();
    this->restoreChanges();
    Q_EMIT changed();
}

void QHexDocument::redo() {
    m_undostack->redo();
    this->restoreChanges();
    Q_EMIT changed();
}

void QHexDocument::insert(qint64 offset, uchar b) {
    this->insert(offset, QByteArray(1, b));
}

void QHexDocument::replace(qint64 offset, uchar b) {
    this->replace(offset, QByteArray(1, b));
}

void QHexDocument::insert(qint64 offset, const QByteArray& data) {
    if(m_trackchanges) {
        m_changes.push_back({
            QHexChangeReason::Insert,
            offset,
            offset + data.size(),
        });

        std::sort(m_changes.begin(), m_changes.end());
        this->moveChanges(offset, data.size());
    }

    m_undostack->push(
        new QHexViewInsertCommand(m_buffer, m_changes, this, offset, data));

    Q_EMIT changed();
    Q_EMIT dataChanged(data, offset, QHexChangeReason::Insert);
}

void QHexDocument::replace(qint64 offset, const QByteArray& data) {
    m_undostack->push(
        new QHexViewReplaceCommand(m_buffer, m_changes, this, offset, data));

    /* NOTE: Mark replacements only if no change has been found */
    if(m_trackchanges && this->findChange(offset) == -1) {
        m_changes.push_back({
            QHexChangeReason::Replace,
            offset,
            offset + data.size(),
        });

        std::sort(m_changes.begin(), m_changes.end());
    }

    Q_EMIT changed();
    Q_EMIT dataChanged(data, offset, QHexChangeReason::Replace);
}

void QHexDocument::remove(qint64 offset, int len) {
    QByteArray data = m_buffer->read(offset, len);
    m_undostack->push(
        new QHexViewRemoveCommand(m_buffer, m_changes, this, offset, len));

    if(m_trackchanges)
        this->removeChange(offset, len);

    Q_EMIT changed();
    Q_EMIT dataChanged(data, offset, QHexChangeReason::Remove);
}

QByteArray QHexDocument::read(qint64 offset, int len) const {
    return m_buffer->read(offset, len);
}

bool QHexDocument::saveTo(QIODevice* device) {
    if(!device->isWritable())
        return false;
    m_buffer->write(device);
    return true;
}

void QHexDocument::removeChange(qint64 offset, qint64 n) {
    QHexChanges newchanges;

    for(const QHexChangeRange& cr : m_changes) {
        if(cr.end <= offset)
            newchanges.push_back(cr); /* before removed range */
        else if(cr.start >= offset + n) {
            newchanges.push_back({
                cr.reason,
                cr.start - n,
                cr.end - n,
            }); /* after: shift back */
        }
        else { /* overlaps */
            if(cr.start < offset)
                newchanges.push_back(
                    {cr.reason, cr.start, offset}); // left part
            if(cr.end > offset + n)
                newchanges.push_back({
                    cr.reason,
                    offset,
                    cr.end - n,
                }); /* right part */
        }
    }

    m_changes.swap(newchanges);
}

void QHexDocument::moveChanges(qint64 offset, qint64 n) {
    qsizetype idx = this->findChange(offset);
    if(idx == -1)
        return;

    for(idx = idx + 1; idx < m_changes.size(); idx++) {
        m_changes[idx].start += n;
        m_changes[idx].end += n;
    }
}

void QHexDocument::restoreChanges() {
    if(!m_trackchanges)
        return;

    const QUndoCommand* cmd = m_undostack->command(m_undostack->index());

    if(cmd)
        m_changes = static_cast<const QHexViewCommand*>(cmd)->changes();
    else
        m_changes.clear();
}

QHexDocument* QHexDocument::fromBuffer(QHexBuffer* buffer, QObject* parent) {
    return new QHexDocument(buffer, parent);
}

QHexDocument* QHexDocument::fromLargeFile(QString filename, QObject* parent) {
    return QHexDocument::fromDevice<QDeviceBuffer>(new QFile(filename), parent);
}

QHexDocument* QHexDocument::fromMappedFile(QString filename, QObject* parent) {
    return QHexDocument::fromDevice<QMappedFileBuffer>(new QFile(filename),
                                                       parent);
}

QHexDocument* QHexDocument::create(QObject* parent) {
    return QHexDocument::fromMemory<QMemoryBuffer>({}, parent);
}
