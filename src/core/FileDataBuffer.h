#ifndef FILEDATABUFFER_H
#define FILEDATABUFFER_H

#include <QObject>
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMutex>

class FileDataBuffer : public QObject
{
    Q_OBJECT

public:
    explicit FileDataBuffer(QObject* parent = nullptr);

    bool openFile(const QString& filePath);
    QString filePath() const;

    /* Get all data (thread-safe) */
    QByteArray data() const;

    /* Read range without materializing the entire file */
    QByteArray read(qint64 pos, qint64 length) const;

    /* Load data as the initial document state */
    void loadData(const QByteArray& data);

    /* Completely replace the working copy of data without resetting the dirty state */
    void replaceData(const QByteArray& data);

    /* Change a single byte */
    void setByte(qint64 pos, char byte);

    /* Get a single byte */
    char getByte(qint64 pos) const;

    /* Change a range of bytes */
    void setBytes(qint64 pos, const QByteArray& bytes);

    /* Buffer size */
    qint64 size() const;

    /* Set selection */
    void setSelection(qint64 pos, qint64 length);

    /* Get current selection */
    void getSelection(qint64& pos, qint64& length) const;

    /* Get hash of the original data (to check for changes) */
    uint originalHash() const;

    /* Get hash of the current data */
    uint currentHash() const;

    /* Check if the data has been modified */
    bool isModified() const;

    /* Reset modification flag (after saving) */
    void markSaved();

    bool saveToFile(const QString& filePath = QString());

    bool isFileBacked() const;
    bool isMaterialized() const;
    bool isLargeFile() const;

signals:
    /* Single byte changed */
    void byteChanged(qint64 pos);

    /* Range of bytes changed */
    void bytesChanged(qint64 pos, qint64 length);

    /* All data changed (e.g., a new file was loaded) */
    void dataChanged();

    /* Selection changed */
    void selectionChanged(qint64 pos, qint64 length);

private:
    static constexpr qint64 kDefaultChunkSize = 64 * 1024;
    static constexpr int kDefaultMaxCachedChunks = 64;
    static constexpr qint64 kLargeFileThreshold = 16 * 1024 * 1024;

    QByteArray readLocked(qint64 pos, qint64 length) const;
    QByteArray materializeLocked() const;
    QByteArray baseReadLocked(qint64 pos, qint64 length) const;
    QByteArray chunkLocked(qint64 chunkIndex) const;
    void touchChunkLocked(qint64 chunkIndex) const;
    void trimChunkCacheLocked() const;
    void promoteToMemoryModeLocked();
    void resetOverlayLocked();
    void closeFileLocked();
    uint computeCurrentHashLocked() const;

    mutable QMutex m_mutex;
    mutable QFile m_file;
    QString m_filePath;
    qint64 m_baseSize = 0;
    qint64 m_chunkSize = kDefaultChunkSize;
    int m_maxCachedChunks = kDefaultMaxCachedChunks;
    bool m_fileBacked = false;
    bool m_materialized = true;
    mutable QHash<qint64, QByteArray> m_chunkCache;
    mutable QList<qint64> m_chunkLru;
    QMap<qint64, char> m_overrides;
    QByteArray m_data;
    uint m_originalHash = 0;
    qint64 m_selectionPos = -1;
    qint64 m_selectionLength = 0;
};

#endif // FILEDATABUFFER_H