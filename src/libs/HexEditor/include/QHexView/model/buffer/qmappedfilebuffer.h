#pragma once

#include <QHexView/model/buffer/qdevicebuffer.h>

class QMappedFileBuffer: public QDeviceBuffer {
public:
    explicit QMappedFileBuffer(QObject* parent = nullptr);
    virtual ~QMappedFileBuffer();

public:
    QByteArray read(qint64 offset, int length) override;
    bool read(QIODevice* iodevice) override;
    void write(QIODevice* iodevice) override;

private:
    void remap();

private:
    uchar* m_mappeddata{nullptr};
};
