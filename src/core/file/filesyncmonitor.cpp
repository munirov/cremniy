#include "filesyncmonitor.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

namespace {

/* Bursty editors often emit several events per logical save */
constexpr int EVENT_DEBOUNCE_MS = 300;

/* Chunk size used while hashing file contents (keeps memory flat) */
constexpr qint64 SIGNATURE_CHUNK_SIZE = 64 * 1024;

} // namespace

FileSyncMonitor::FileSyncMonitor(QObject* parent)
    : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(EVENT_DEBOUNCE_MS);

    connect(
        m_watcher,
        &QFileSystemWatcher::fileChanged,
        this,
        &FileSyncMonitor::onSourceFileChanged
    );

    connect(
        m_debounceTimer,
        &QTimer::timeout,
        this,
        &FileSyncMonitor::flushPendingEvents
    );
}

void FileSyncMonitor::watch(const QString& filePath)
{
    if (filePath.isEmpty())
        return;

    if (!m_baselines.contains(filePath))
        refreshBaseline(filePath);

    if (!m_watcher->addPath(filePath))
        qDebug() << "FileSyncMonitor: failed to watch" << filePath;
}

void FileSyncMonitor::unwatch(const QString& filePath)
{
    m_pendingPaths.remove(filePath);
    m_baselines.remove(filePath);
    m_watcher->removePath(filePath);
}

void FileSyncMonitor::refreshBaseline(const QString& filePath)
{
    const uint signature = computeContentSignature(filePath);
    if (signature != 0 || QFileInfo(filePath).exists())
        m_baselines.insert(filePath, signature);
    else
        m_baselines.remove(filePath);
}

void FileSyncMonitor::onSourceFileChanged(const QString& filePath)
{
    if (!m_baselines.contains(filePath)) {
        /*
         * A tracked path may be recreated after deletion. Sync silently with
         * the new contents and resume watching instead of reporting a change
         * against a stale baseline.
         */
        refreshBaseline(filePath);
        return;
    }

    m_pendingPaths.insert(filePath);

    /* Coalesce bursty notifications into a single check */
    m_debounceTimer->start();
}

void FileSyncMonitor::flushPendingEvents()
{
    const QSet<QString> pending = m_pendingPaths;
    m_pendingPaths.clear();

    for (const QString& filePath : pending) {
        if (!m_baselines.contains(filePath))
            continue;

        const QFileInfo info(filePath);
        const bool watched = m_watcher->files().contains(filePath);
        if (!watched && info.exists()) {
            m_watcher->addPath(filePath);
        }

        if (!info.exists()) {
            m_baselines.remove(filePath);
            emit fileDisappeared(filePath);
            continue;
        }

        const uint signature = computeContentSignature(filePath);
        if (signature == m_baselines.value(filePath))
            continue; /* own write or no-op touch */

        /*
         * Adopt the new on-disk state before notifying, so repeated
         * notifications for the same change do not fire twice.
         */
        m_baselines.insert(filePath, signature);
        emit fileChangedOnDisk(filePath);
    }
}

uint FileSyncMonitor::computeContentSignature(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return 0;

    uint seed = 0;
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(SIGNATURE_CHUNK_SIZE);
        if (chunk.isEmpty())
            break;

        seed = qHash(chunk, seed);
    }
    return seed;
}
