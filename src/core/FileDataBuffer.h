#ifndef FILEDATABUFFER_H
#define FILEDATABUFFER_H

#include <list>
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

    // Получить все данные (потокобезопасно)
    QByteArray data() const;

    // Прочитать диапазон без материализации всего файла
    QByteArray read(qint64 pos, qint64 length) const;

    // Загрузить данные как исходное состояние документа
    void loadData(const QByteArray& data);

    // Полностью заменить рабочую копию данных, не сбрасывая dirty-state
    void replaceData(const QByteArray& data);

    // Изменить один байт
    void setByte(qint64 pos, char byte);

    // Получить один байт
    char getByte(qint64 pos) const;

    // Изменить диапазон байтов
    void setBytes(qint64 pos, const QByteArray& bytes);

    // Размер буфера
    qint64 size() const;

    // Установить выделение
    void setSelection(qint64 pos, qint64 length);

    // Получить текущее выделение
    void getSelection(qint64& pos, qint64& length) const;

    // Получить хеш оригинальных данных (для проверки изменений)
    uint originalHash() const;

    // Получить хеш текущих данных
    uint currentHash() const;

    // Проверить, изменены ли данные
    bool isModified() const;

    // Сбросить флаг изменений (после сохранения)
    void markSaved();

    bool saveToFile(const QString& filePath = QString());

    bool isFileBacked() const;
    bool isMaterialized() const;
    bool isLargeFile() const;

signals:
    // Изменился один байт
    void byteChanged(qint64 pos);

    // Изменился диапазон байтов
    void bytesChanged(qint64 pos, qint64 length);

    // Изменились все данные (например загружен новый файл)
    void dataChanged();

    // Изменилось выделение
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
    mutable std::list<qint64> m_chunkLruList;
    mutable QHash<qint64, std::list<qint64>::iterator> m_chunkLruIter;
    mutable QHash<qint64, QByteArray> m_chunkCache;
    QMap<qint64, char> m_overrides;
    QByteArray m_data;
    uint m_originalHash = 0;
    qint64 m_selectionPos = -1;
    qint64 m_selectionLength = 0;
};

#endif // FILEDATABUFFER_H
