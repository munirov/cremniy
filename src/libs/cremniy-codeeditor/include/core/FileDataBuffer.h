#ifndef FILEDATABUFFER_H
#define FILEDATABUFFER_H

#include <QObject>
#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QVector>

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

    // Вставить или удалить диапазон байтов
    void insertBytes(qint64 pos, const QByteArray& bytes);
    void removeBytes(qint64 pos, qint64 length);

    // Отменить/повторить последнее изменение
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    void beginHistoryGroup();
    void endHistoryGroup();

    // Сохраняет состояние выделения/курсора ДО/ПОСЛЕ применения изменения.
    void setPendingHistorySelectionBefore(qint64 pos, qint64 length);
    void setPendingHistorySelectionAfter(qint64 pos, qint64 length);

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

    qint64 indexOf(const QByteArray& needle, qint64 from = 0) const;
    qint64 lastIndexOf(const QByteArray& needle, qint64 from = -1) const;

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
    void applyDataSnapshotLocked(const QByteArray& data);
    void pushHistoryLocked(const QByteArray& before, const QByteArray& after);
    void endHistoryGroupLocked();

    struct SelectionState {
        qint64 pos = 0;
        qint64 length = 0;
    };

    struct HistoryEntry {
        QByteArray before;
        QByteArray after;
        SelectionState selectionBefore;
        SelectionState selectionAfter;
    };

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
    QVector<HistoryEntry> m_undoStack;
    QVector<HistoryEntry> m_redoStack;
    int m_maxHistoryEntries = 100;
    bool m_historyGroupActive = false;
    QByteArray m_historyGroupBefore;
    QByteArray m_historyGroupAfter;

    // Временное состояние выделения ДО/ПОСЛЕ изменения(history entry)
    SelectionState m_historyGroupSelectionBefore;
    SelectionState m_historyGroupSelectionAfter;
    SelectionState m_pendingSelectionBefore;
    SelectionState m_pendingSelectionAfter;
    // Флаги, показывающие, что значения выше были явно установлены
    bool m_hasPendingSelectionBefore = false;
    bool m_hasPendingSelectionAfter = false;
};

#endif // FILEDATABUFFER_H
