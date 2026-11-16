#ifndef FILESYNCMONITOR_H
#define FILESYNCMONITOR_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QFileSystemWatcher;
class QTimer;

/**
 * @brief Tracks on-disk changes for files opened in the editor
 *
 * The monitor wraps QFileSystemWatcher and adds the pieces required for a
 * reliable "sync with disk" workflow:
 *  - debouncing of bursty file system events (editors often produce several
 *    notifications for a single logical save);
 *  - keeping a "known on-disk content" signature (baseline) per watched file,
 *    so writes performed by the application itself are silently absorbed and
 *    do not produce false "external change" reports;
 *  - restoring dropped watches (some platforms remove a file from the watcher
 *    once it has been changed or replaced on disk);
 *  - a distinct notification for files that vanished (deleted or renamed
 *    outside the editor).
 */
class FileSyncMonitor : public QObject
{
    Q_OBJECT

public:
    explicit FileSyncMonitor(QObject* parent = nullptr);

    /**
     * @brief Start watching a file
     *
     * The current on-disk content is adopted as the baseline. Watching an
     * already tracked file does not re-prime its baseline, so pending change
     * notifications are not lost.
     */
    void watch(const QString& filePath);

    /**
     * @brief Stop watching a file completely
     */
    void unwatch(const QString& filePath);

    /**
     * @brief Adopt the current on-disk content as known
     *
     * Must be called after the application itself writes or reloads the file,
     * so that such writes do not show up as external changes.
     */
    void refreshBaseline(const QString& filePath);

signals:
    /**
     * @brief File content on disk differs from the last known state
     * @param filePath absolute path of the changed file
     */
    void fileChangedOnDisk(const QString& filePath);

    /**
     * @brief Watched file no longer exists on disk
     *
     * Covers both deletion and renaming outside the editor.
     *
     * @param filePath absolute path of the vanished file
     */
    void fileDisappeared(const QString& filePath);

private slots:
    void onSourceFileChanged(const QString& filePath);
    void flushPendingEvents();

private:
    /* Signature of the current file contents; 0 also means "unreadable" */
    static uint computeContentSignature(const QString& filePath);

    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_debounceTimer = nullptr;
    QSet<QString> m_pendingPaths;
    QHash<QString, uint> m_baselines;
};

#endif // FILESYNCMONITOR_H