#include "FileDataBuffer.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSaveFile>

FileDataBuffer::FileDataBuffer(QObject* parent)
    : QObject(parent)
{
}

bool FileDataBuffer::openFile(const QString& filePath)
{
    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile())
        return false;

    QMutexLocker locker(&m_mutex);
    closeFileLocked();
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        m_file.setFileName(QString());
        return false;
    }

    m_filePath = filePath;
    m_baseSize = m_file.size();
    if (m_baseSize < kLargeFileThreshold) {
        m_data = m_file.readAll();
        m_file.close();
        m_fileBacked = false;
        m_materialized = true;
        m_baseSize = m_data.size();
    } else {
        m_fileBacked = true;
        m_materialized = false;
        m_data.clear();
    }
    m_chunkCache.clear();
    m_chunkLru.clear();
    resetOverlayLocked();
    m_undoStack.clear();
    m_redoStack.clear();
    m_originalHash = computeCurrentHashLocked();
    locker.unlock();

    emit dataChanged();
    return true;
}

QString FileDataBuffer::filePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_filePath;
}

QByteArray FileDataBuffer::data() const
{
    QMutexLocker locker(&m_mutex);
    return materializeLocked();
}

QByteArray FileDataBuffer::read(qint64 pos, qint64 length) const
{
    QMutexLocker locker(&m_mutex);
    return readLocked(pos, length);
}

void FileDataBuffer::loadData(const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);
    closeFileLocked();
    m_filePath.clear();
    m_data = data;
    m_baseSize = m_data.size();
    m_fileBacked = false;
    m_materialized = true;
    resetOverlayLocked();
    m_undoStack.clear();
    m_redoStack.clear();
    m_originalHash = qHash(data, 0);
    locker.unlock();
    emit dataChanged();
}

void FileDataBuffer::replaceData(const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);
    endHistoryGroupLocked();
    const QByteArray before = materializeLocked();
    if (before == data)
        return;

    pushHistoryLocked(before, data);
    applyDataSnapshotLocked(data);
    locker.unlock();
    emit dataChanged();
}

void FileDataBuffer::insertBytes(qint64 pos, const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return;

    QMutexLocker locker(&m_mutex);
    const QByteArray before = materializeLocked();
    QByteArray after = before;
    pos = qBound<qint64>(0, pos, after.size());
    after.insert(static_cast<int>(pos), bytes);
    pushHistoryLocked(before, after);
    applyDataSnapshotLocked(after);
    locker.unlock();
    emit dataChanged();
}

void FileDataBuffer::removeBytes(qint64 pos, qint64 length)
{
    if (length <= 0)
        return;

    QMutexLocker locker(&m_mutex);
    const QByteArray before = materializeLocked();
    if (pos < 0 || pos >= before.size())
        return;

    const qint64 boundedLength = qMin<qint64>(length, before.size() - pos);
    QByteArray after = before;
    after.remove(static_cast<int>(pos), static_cast<int>(boundedLength));
    pushHistoryLocked(before, after);
    applyDataSnapshotLocked(after);
    locker.unlock();
    emit dataChanged();
}

void FileDataBuffer::undo()
{
    QMutexLocker locker(&m_mutex);
    endHistoryGroupLocked();
    if (m_undoStack.isEmpty())
        return;

    const HistoryEntry entry = m_undoStack.takeLast();
    m_redoStack.append(entry);
    applyDataSnapshotLocked(entry.before);
    locker.unlock();
    emit dataChanged();
}

void FileDataBuffer::redo()
{
    QMutexLocker locker(&m_mutex);
    endHistoryGroupLocked();
    if (m_redoStack.isEmpty())
        return;

    const HistoryEntry entry = m_redoStack.takeLast();
    m_undoStack.append(entry);
    applyDataSnapshotLocked(entry.after);
    locker.unlock();
    emit dataChanged();
}

bool FileDataBuffer::canUndo() const
{
    QMutexLocker locker(&m_mutex);
    return !m_undoStack.isEmpty();
}

bool FileDataBuffer::canRedo() const
{
    QMutexLocker locker(&m_mutex);
    return !m_redoStack.isEmpty();
}

void FileDataBuffer::beginHistoryGroup()
{
    QMutexLocker locker(&m_mutex);
    if (m_historyGroupActive)
        return;

    m_historyGroupActive = true;
    m_historyGroupBefore.clear();
    m_historyGroupAfter.clear();
}

void FileDataBuffer::endHistoryGroup()
{
    QMutexLocker locker(&m_mutex);
    endHistoryGroupLocked();
}

void FileDataBuffer::endHistoryGroupLocked()
{
    if (!m_historyGroupActive)
        return;

    if (!m_historyGroupBefore.isEmpty() || !m_historyGroupAfter.isEmpty()) {
        m_undoStack.append({m_historyGroupBefore, m_historyGroupAfter});
        if (m_undoStack.size() > m_maxHistoryEntries)
            m_undoStack.remove(0, m_undoStack.size() - m_maxHistoryEntries);
        m_redoStack.clear();
    }

    m_historyGroupActive = false;
    m_historyGroupBefore.clear();
    m_historyGroupAfter.clear();
}

void FileDataBuffer::setByte(qint64 pos, char byte)
{
    QMutexLocker locker(&m_mutex);
    endHistoryGroupLocked();
    const QByteArray before = materializeLocked();
    const qint64 totalSize = before.size();
    if (pos < 0 || pos >= totalSize)
        return;

    QByteArray after = before;
    if (after[pos] == byte)
        return;

    after[pos] = byte;
    pushHistoryLocked(before, after);
    applyDataSnapshotLocked(after);

    locker.unlock();
    emit byteChanged(pos);
    emit dataChanged();
}

char FileDataBuffer::getByte(qint64 pos) const
{
    const QByteArray bytes = read(pos, 1);
    return bytes.isEmpty() ? 0 : bytes[0];
}

void FileDataBuffer::setBytes(qint64 pos, const QByteArray& bytes)
{
    if (bytes.isEmpty())
        return;

    QMutexLocker locker(&m_mutex);
    endHistoryGroupLocked();
    const QByteArray before = materializeLocked();
    const qint64 totalSize = before.size();
    if (pos < 0 || pos >= totalSize)
        return;

    const qint64 maxLength = qMin<qint64>(bytes.size(), totalSize - pos);
    if (maxLength <= 0)
        return;

    QByteArray after = before;
    bool changed = false;
    for (qint64 i = 0; i < maxLength; ++i) {
        if (after[pos + i] != bytes[i]) {
            after[pos + i] = bytes[i];
            changed = true;
        }
    }

    if (!changed)
        return;

    pushHistoryLocked(before, after);
    applyDataSnapshotLocked(after);

    locker.unlock();
    emit bytesChanged(pos, maxLength);
    emit dataChanged();
}

qint64 FileDataBuffer::size() const
{
    QMutexLocker locker(&m_mutex);
    return m_materialized ? m_data.size() : m_baseSize;
}

void FileDataBuffer::setSelection(qint64 pos, qint64 length)
{
    QMutexLocker locker(&m_mutex);
    if (m_selectionPos == pos && m_selectionLength == length)
        return;

    m_selectionPos = pos;
    m_selectionLength = length;
    locker.unlock();
    emit selectionChanged(pos, length);
}

void FileDataBuffer::getSelection(qint64& pos, qint64& length) const
{
    QMutexLocker locker(&m_mutex);
    pos = m_selectionPos;
    length = m_selectionLength;
}

uint FileDataBuffer::originalHash() const
{
    QMutexLocker locker(&m_mutex);
    return m_originalHash;
}

uint FileDataBuffer::currentHash() const
{
    QMutexLocker locker(&m_mutex);
    return computeCurrentHashLocked();
}

bool FileDataBuffer::isModified() const
{
    QMutexLocker locker(&m_mutex);
    return computeCurrentHashLocked() != m_originalHash;
}

void FileDataBuffer::markSaved()
{
    QMutexLocker locker(&m_mutex);
    m_originalHash = computeCurrentHashLocked();
}

bool FileDataBuffer::saveToFile(const QString& filePath)
{
    QString targetPath;
    QByteArray payload;
    bool keepFileBacked = false;
    bool sourceWasOpen = false;
    {
        QMutexLocker locker(&m_mutex);
        targetPath = filePath.isEmpty() ? m_filePath : filePath;
        payload = materializeLocked();
        keepFileBacked = m_fileBacked;
        sourceWasOpen = m_file.isOpen();
        if (sourceWasOpen)
            m_file.close();
    }

    if (targetPath.isEmpty())
        return false;

    QSaveFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (sourceWasOpen) {
            QMutexLocker locker(&m_mutex);
            m_file.setFileName(m_filePath);
            const bool reopened = m_file.open(QIODevice::ReadOnly);
            Q_UNUSED(reopened);
        }
        return false;
    }

    if (out.write(payload) != payload.size()) {
        if (sourceWasOpen) {
            QMutexLocker locker(&m_mutex);
            m_file.setFileName(m_filePath);
            const bool reopened = m_file.open(QIODevice::ReadOnly);
            Q_UNUSED(reopened);
        }
        return false;
    }

    if (!out.commit()) {
        if (sourceWasOpen) {
            QMutexLocker locker(&m_mutex);
            m_file.setFileName(m_filePath);
            const bool reopened = m_file.open(QIODevice::ReadOnly);
            Q_UNUSED(reopened);
        }
        return false;
    }

    {
        QMutexLocker locker(&m_mutex);
        closeFileLocked();
        m_filePath = targetPath;
        m_baseSize = payload.size();
        resetOverlayLocked();

        if (keepFileBacked || payload.size() >= kLargeFileThreshold) {
            m_file.setFileName(targetPath);
            if (m_file.open(QIODevice::ReadOnly)) {
                m_data.clear();
                m_fileBacked = true;
                m_materialized = false;
            } else {
                m_data = payload;
                m_fileBacked = false;
                m_materialized = true;
            }
        } else {
            m_data = payload;
            m_fileBacked = false;
            m_materialized = true;
        }

        m_originalHash = computeCurrentHashLocked();
    }

    return true;
}

qint64 FileDataBuffer::indexOf(const QByteArray& needle, qint64 from) const
{
    if (needle.isEmpty())
        return -1;

    return data().indexOf(needle, static_cast<int>(qMax<qint64>(0, from)));
}

qint64 FileDataBuffer::lastIndexOf(const QByteArray& needle, qint64 from) const
{
    if (needle.isEmpty())
        return -1;

    const QByteArray current = data();
    if (current.isEmpty())
        return -1;

    const int start = from < 0 ? current.size() - 1 : static_cast<int>(qMin<qint64>(from, current.size() - 1));
    return current.lastIndexOf(needle, start);
}

bool FileDataBuffer::isFileBacked() const
{
    QMutexLocker locker(&m_mutex);
    return m_fileBacked;
}

bool FileDataBuffer::isMaterialized() const
{
    QMutexLocker locker(&m_mutex);
    return m_materialized;
}

bool FileDataBuffer::isLargeFile() const
{
    QMutexLocker locker(&m_mutex);
    return (m_materialized ? m_data.size() : m_baseSize) >= kLargeFileThreshold;
}

QByteArray FileDataBuffer::readLocked(qint64 pos, qint64 length) const
{
    const qint64 totalSize = m_materialized ? m_data.size() : m_baseSize;
    if (pos < 0 || length <= 0 || pos >= totalSize)
        return {};

    const qint64 boundedLength = qMin<qint64>(length, totalSize - pos);
    if (m_materialized)
        return m_data.mid(pos, boundedLength);

    QByteArray result = baseReadLocked(pos, boundedLength);
    for (auto it = m_overrides.lowerBound(pos); it != m_overrides.end() && it.key() < pos + boundedLength; ++it)
        result[it.key() - pos] = it.value();
    return result;
}

QByteArray FileDataBuffer::materializeLocked() const
{
    if (m_materialized)
        return m_data;

    QByteArray result;
    result.reserve(m_baseSize);
    qint64 offset = 0;
    while (offset < m_baseSize) {
        const qint64 len = qMin<qint64>(m_chunkSize, m_baseSize - offset);
        result.append(readLocked(offset, len));
        offset += len;
    }
    return result;
}

QByteArray FileDataBuffer::baseReadLocked(qint64 pos, qint64 length) const
{
    if (!m_fileBacked || !m_file.isOpen() || length <= 0)
        return {};

    QByteArray result;
    result.reserve(length);
    const qint64 startChunk = pos / m_chunkSize;
    const qint64 endChunk = (pos + length - 1) / m_chunkSize;

    qint64 remaining = length;
    qint64 currentPos = pos;
    for (qint64 chunkIndex = startChunk; chunkIndex <= endChunk && remaining > 0; ++chunkIndex) {
        const QByteArray chunk = chunkLocked(chunkIndex);
        if (chunk.isEmpty())
            break;

        const qint64 chunkStart = chunkIndex * m_chunkSize;
        const qint64 offsetInChunk = currentPos - chunkStart;
        const qint64 take = qMin<qint64>(remaining, chunk.size() - offsetInChunk);
        result.append(chunk.constData() + offsetInChunk, take);
        currentPos += take;
        remaining -= take;
    }

    return result;
}

QByteArray FileDataBuffer::chunkLocked(qint64 chunkIndex) const
{
    if (m_chunkCache.contains(chunkIndex)) {
        touchChunkLocked(chunkIndex);
        return m_chunkCache.value(chunkIndex);
    }

    if (!m_file.seek(chunkIndex * m_chunkSize))
        return {};

    QByteArray chunk = m_file.read(m_chunkSize);
    m_chunkCache.insert(chunkIndex, chunk);
    touchChunkLocked(chunkIndex);
    trimChunkCacheLocked();
    return chunk;
}

void FileDataBuffer::touchChunkLocked(qint64 chunkIndex) const
{
    m_chunkLru.removeAll(chunkIndex);
    m_chunkLru.prepend(chunkIndex);
}

void FileDataBuffer::trimChunkCacheLocked() const
{
    while (m_chunkLru.size() > m_maxCachedChunks) {
        const qint64 chunkIndex = m_chunkLru.takeLast();
        m_chunkCache.remove(chunkIndex);
    }
}

void FileDataBuffer::promoteToMemoryModeLocked()
{
    if (m_materialized)
        return;

    m_data = materializeLocked();
    m_baseSize = m_data.size();
    m_materialized = true;
    m_fileBacked = false;
    closeFileLocked();
    resetOverlayLocked();
}

void FileDataBuffer::resetOverlayLocked()
{
    m_overrides.clear();
}

void FileDataBuffer::closeFileLocked()
{
    if (m_file.isOpen())
        m_file.close();
    m_chunkCache.clear();
    m_chunkLru.clear();
    m_fileBacked = false;
    m_materialized = true;
    m_baseSize = m_data.size();
}

void FileDataBuffer::applyDataSnapshotLocked(const QByteArray& data)
{
    m_data = data;
    closeFileLocked();
    m_materialized = true;
    m_fileBacked = false;
    m_baseSize = m_data.size();
    resetOverlayLocked();

    if (m_selectionPos > m_data.size())
        m_selectionPos = m_data.size();
    if (m_selectionPos < 0)
        m_selectionLength = 0;
    else
        m_selectionLength = qBound<qint64>(0, m_selectionLength, m_data.size() - m_selectionPos);
}

void FileDataBuffer::pushHistoryLocked(const QByteArray& before, const QByteArray& after)
{
    if (m_historyGroupActive) {
        if (m_historyGroupBefore.isEmpty() && m_historyGroupAfter.isEmpty())
            m_historyGroupBefore = before;
        m_historyGroupAfter = after;
        m_redoStack.clear();
        return;
    }

    m_undoStack.append({before, after});
    if (m_undoStack.size() > m_maxHistoryEntries)
        m_undoStack.remove(0, m_undoStack.size() - m_maxHistoryEntries);
    m_redoStack.clear();
}

uint FileDataBuffer::computeCurrentHashLocked() const
{
    if (m_materialized)
        return qHash(m_data, 0);

    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 offset = 0;
    while (offset < m_baseSize) {
        const qint64 len = qMin<qint64>(m_chunkSize, m_baseSize - offset);
        hash.addData(readLocked(offset, len));
        offset += len;
    }

    const QByteArray digest = hash.result();
    uint value = 0;
    memcpy(&value, digest.constData(), qMin<int>(sizeof(value), digest.size()));
    return value;
}
