#pragma once

#include <QHexView/model/buffer/qhexbuffer.h>

class FileDataBuffer;

class QFileDataBuffer: public QHexBuffer {
    Q_OBJECT

public:
    explicit QFileDataBuffer(FileDataBuffer* buffer, QObject* parent = nullptr);

    uchar at(qint64 idx) override;
    bool accept(qint64 idx) const override;
    qint64 length() const override;
    void insert(qint64 offset, const QByteArray& data) override;
    void remove(qint64 offset, int length) override;
    void replace(qint64 offset, const QByteArray& data) override;
    QByteArray read(qint64 offset, int length) override;
    bool read(QIODevice* iodevice) override;
    void write(QIODevice* iodevice) override;
    qint64 indexOf(const QByteArray& ba, qint64 from) override;
    qint64 lastIndexOf(const QByteArray& ba, qint64 from) override;

    FileDataBuffer* sharedBuffer() const;

private:
    FileDataBuffer* m_buffer;
    bool m_ignoreExternalSignals = false;
};
