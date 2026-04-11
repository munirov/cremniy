#include <QFile>
#include <QHexView/model/buffer/qmappedfilebuffer.h>

QMappedFileBuffer::QMappedFileBuffer(QObject* parent): QDeviceBuffer{parent} {}

QMappedFileBuffer::~QMappedFileBuffer() {
    if((m_device && (m_device->parent() == this)) && m_mappeddata) {
        QFile* f = qobject_cast<QFile*>(m_device);
        f->unmap(m_mappeddata);
    }

    m_mappeddata = nullptr;
}

QByteArray QMappedFileBuffer::read(qint64 offset, int length) {
    if(offset >= this->length())
        return {};

    if(offset + length >= this->length())
        length = this->length() - offset;

    return QByteArray::fromRawData(
        reinterpret_cast<const char*>(m_mappeddata + offset), length);
}

bool QMappedFileBuffer::read(QIODevice* iodevice) {
    m_device = qobject_cast<QFile*>(iodevice);
    if(!m_device || !QDeviceBuffer::read(iodevice))
        return false;

    this->remap();
    return m_mappeddata;
}

void QMappedFileBuffer::write(QIODevice* iodevice) {
    if(iodevice == m_device)
        this->remap();
    else
        iodevice->write(reinterpret_cast<const char*>(m_mappeddata),
                        m_device->size());
}

void QMappedFileBuffer::remap() {
    QFile* f = qobject_cast<QFile*>(m_device);
    if(!f)
        return;

    if(m_mappeddata)
        f->unmap(m_mappeddata);
    m_mappeddata = f->map(0, f->size());
}
