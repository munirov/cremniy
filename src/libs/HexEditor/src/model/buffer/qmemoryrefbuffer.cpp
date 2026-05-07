#include <QBuffer>
#include <QHexView/model/buffer/qmemoryrefbuffer.h>

QMemoryRefBuffer::QMemoryRefBuffer(QObject* parent): QDeviceBuffer{parent} {}

bool QMemoryRefBuffer::read(QIODevice* device) {
    m_device = qobject_cast<QBuffer*>(device);

    if(m_device) {
        m_device->setParent(this);
        return QDeviceBuffer::read(device);
    }

    return false;
}

void QMemoryRefBuffer::write(QIODevice* device) {
    if(!m_device || m_device == device)
        return;

    static const int CHUNK_SIZE = 4096;
    m_device->seek(0);

    while(!m_device->atEnd())
        device->write(m_device->read(CHUNK_SIZE));
}
