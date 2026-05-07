#include <QHexView/model/buffer/qfiledatabuffer.h>

#include "core/file/FileDataBuffer.h"

#include <QIODevice>

QFileDataBuffer::QFileDataBuffer(FileDataBuffer* buffer, QObject* parent)
    : QHexBuffer(parent)
    , m_buffer(buffer)
{
    Q_ASSERT(m_buffer);

    connect(m_buffer, &FileDataBuffer::dataChanged, this, [this]() {
        if (!m_ignoreExternalSignals)
            emit dataChangedExternally(0, m_buffer->size());
    });
    connect(m_buffer, &FileDataBuffer::byteChanged, this, [this](qint64 pos) {
        if (!m_ignoreExternalSignals)
            emit dataChangedExternally(pos, 1);
    });
    connect(m_buffer, &FileDataBuffer::bytesChanged, this, [this](qint64 pos, qint64 length) {
        if (!m_ignoreExternalSignals)
            emit dataChangedExternally(pos, length);
    });
}

uchar QFileDataBuffer::at(qint64 idx)
{
    return static_cast<uchar>(m_buffer->getByte(idx));
}

bool QFileDataBuffer::accept(qint64 idx) const
{
    return idx >= 0 && idx < m_buffer->size();
}

qint64 QFileDataBuffer::length() const
{
    return m_buffer->size();
}

void QFileDataBuffer::insert(qint64 offset, const QByteArray& data)
{
    m_ignoreExternalSignals = true;
    m_buffer->insertBytes(offset, data);
    m_ignoreExternalSignals = false;
}

void QFileDataBuffer::remove(qint64 offset, int length)
{
    m_ignoreExternalSignals = true;
    m_buffer->removeBytes(offset, length);
    m_ignoreExternalSignals = false;
}

void QFileDataBuffer::replace(qint64 offset, const QByteArray& data)
{
    m_ignoreExternalSignals = true;
    m_buffer->setBytes(offset, data);
    m_ignoreExternalSignals = false;
}

QByteArray QFileDataBuffer::read(qint64 offset, int length)
{
    return m_buffer->read(offset, length);
}

bool QFileDataBuffer::read(QIODevice* iodevice)
{
    if (!iodevice)
        return false;

    m_ignoreExternalSignals = true;
    m_buffer->loadData(iodevice->readAll());
    m_ignoreExternalSignals = false;
    return true;
}

void QFileDataBuffer::write(QIODevice* iodevice)
{
    if (!iodevice)
        return;

    const qint64 total = m_buffer->size();
    qint64 offset = 0;
    constexpr qint64 chunkSize = 64 * 1024;
    while (offset < total) {
        const qint64 length = qMin(chunkSize, total - offset);
        iodevice->write(m_buffer->read(offset, length));
        offset += length;
    }
}

qint64 QFileDataBuffer::indexOf(const QByteArray& ba, qint64 from)
{
    return m_buffer->indexOf(ba, from);
}

qint64 QFileDataBuffer::lastIndexOf(const QByteArray& ba, qint64 from)
{
    return m_buffer->lastIndexOf(ba, from);
}

FileDataBuffer* QFileDataBuffer::sharedBuffer() const
{
    return m_buffer;
}
