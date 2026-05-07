#pragma once

#include <QHexView/model/buffer/qhexbuffer.h>
#include <QHexView/model/qhexchanges.h>
#include <QHexView/model/qhexmetadata.h>
#include <QUndoStack>

class QHexCursor;
class FileDataBuffer;

class QHexDocument: public QObject {
    Q_OBJECT

public:
    enum class FindDirection { Forward, Backward };
    Q_ENUM(FindDirection);

private:
    explicit QHexDocument(QHexBuffer* buffer, QObject* parent = nullptr);
    qsizetype findChange(qint64 offset) const;
    bool accept(qint64 idx) const;
    void removeChange(qint64 offset, qint64 n);
    void moveChanges(qint64 offset, qint64 n);
    void restoreChanges();

public:
    QHexChangeReason getChangeReason(qint64 offset) const;
    bool isEmpty() const;
    bool isModified() const;
    bool canUndo() const;
    bool canRedo() const;
    bool trackChanges() const;
    void setData(const QByteArray& ba);
    void setData(QHexBuffer* buffer);
    void setTrackChanges(bool b);
    qint64 length() const;
    qint64 indexOf(const QByteArray& ba, qint64 from = 0);
    qint64 lastIndexOf(const QByteArray& ba, qint64 from = 0);
    QByteArray read(qint64 offset, int len = 0) const;
    uchar at(int offset) const;

public Q_SLOTS:
    void clearChanges();
    void clearModified();
    void undo();
    void redo();
    void insert(qint64 offset, uchar b);
    void replace(qint64 offset, uchar b);
    void insert(qint64 offset, const QByteArray& data);
    void replace(qint64 offset, const QByteArray& data);
    void remove(qint64 offset, int len);
    bool saveTo(QIODevice* device);

public:
    template<typename T, bool Owned = true>
    static QHexDocument* fromDevice(QIODevice* iodevice,
                                    QObject* parent = nullptr);
    template<typename T>
    static QHexDocument* fromMemory(char* data, int size,
                                    QObject* parent = nullptr);
    template<typename T>
    static QHexDocument* fromMemory(const QByteArray& ba,
                                    QObject* parent = nullptr);
    static QHexDocument* fromBuffer(QHexBuffer* buffer,
                                    QObject* parent = nullptr);
    static QHexDocument* fromSharedBuffer(FileDataBuffer* buffer,
                                          QObject* parent = nullptr);
    static QHexDocument* fromLargeFile(QString filename,
                                       QObject* parent = nullptr);
    static QHexDocument* fromMappedFile(QString filename,
                                        QObject* parent = nullptr);
    static QHexDocument* fromFile(QString filename, QObject* parent = nullptr);
    static QHexDocument* create(QObject* parent = nullptr);

Q_SIGNALS:
    void trackChangesChanged(bool trackchanges);
    void modifiedChanged(bool modified);
    void canUndoChanged(bool canundo);
    void canRedoChanged(bool canredo);
    void dataChanged(const QByteArray& data, quint64 offset,
                     QHexChangeReason reason);
    void changed();
    void reset();

private:
    QHexBuffer* m_buffer;
    QUndoStack* m_undostack;
    QHexChanges m_changes;
    bool m_trackchanges{false};

    friend class QHexView;
};

template<typename T, bool Owned>
QHexDocument* QHexDocument::fromDevice(QIODevice* iodevice, QObject* parent) {
    QHexBuffer* hexbuffer = new T(parent);
    if(Owned)
        iodevice->setParent(hexbuffer);
    return hexbuffer->read(iodevice) ? new QHexDocument(hexbuffer, parent)
                                     : nullptr;
}

template<typename T>
QHexDocument* QHexDocument::fromMemory(char* data, int size, QObject* parent) {
    QHexBuffer* hexbuffer = new T();
    hexbuffer->read(data, size);
    return new QHexDocument(hexbuffer, parent);
}

template<typename T>
QHexDocument* QHexDocument::fromMemory(const QByteArray& ba, QObject* parent) {
    QHexBuffer* hexbuffer = new T();
    hexbuffer->read(ba);
    return new QHexDocument(hexbuffer, parent);
}
