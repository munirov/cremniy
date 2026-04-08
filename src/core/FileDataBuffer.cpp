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
    m_fileBacked = true;
    m_materialized = false;
    m_data.clear();
    m_chunkCache.clear();
    m_chunkLruList.clear();
    m_chunkLruIter.clear();
    resetOverlayLocked();
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
    m_originalHash = qHash(data, 0);
    locker.unlock();
    emit dataChanged();
}

void FileDataBuffer::replaceData(const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);
    if (m_fileBacked && !m_materialized)
        promoteToMemoryModeLocked();

    if (m_data.size() == data.size() && m_data == data)
        return;

    m_data = data;
    m_baseSize = m_data.size();
    locker.unlock();
    emit dataChanged();
}

void FileDataBuffer::setByte(qint64 pos, char byte)
{
    QMutexLocker locker(&m_mutex);
    const qint64 totalSize = m_materialized ? m_data.size() : m_baseSize;
    if (pos < 0 || pos >= totalSize)
        return;

    if (m_fileBacked && !m_materialized) {
        const QByteArray current = readLocked(pos, 1);
        if (current.size() == 1 && current[0] == byte)
            return;

        m_overrides[pos] = byte;
    } else {
        if (m_data[pos] == byte)
            return;
        m_data[pos] = byte;
    }

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
    const qint64 totalSize = m_materialized ? m_data.size() : m_baseSize;
    if (pos < 0 || pos >= totalSize)
        return;

    const qint64 maxLength = qMin<qint64>(bytes.size(), totalSize - pos);
    if (maxLength <= 0)
        return;

    bool changed = false;
    if (m_fileBacked && !m_materialized) {
        const QByteArray current = readLocked(pos, maxLength);
        for (qint64 i = 0; i < maxLength; ++i) {
            if (i >= current.size() || current[i] != bytes[i]) {
                m_overrides[pos + i] = bytes[i];
                changed = true;
            }
        }
    } else {
        for (qint64 i = 0; i < maxLength; ++i) {
            if (m_data[pos + i] != bytes[i]) {
                m_data[pos + i] = bytes[i];
                changed = true;
            }
        }
    }

    if (!changed)
        return;

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
    bool sourceWasOpen  = false;
    {
        QMutexLocker locker(&m_mutex);
        targetPath    = filePath.isEmpty() ? m_filePath : filePath;
        payload       = materializeLocked();
        keepFileBacked = m_fileBacked;
        sourceWasOpen  = m_file.isOpen();
        if (sourceWasOpen)
            m_file.close();
    }

    if (targetPath.isEmpty())
        return false;

    auto reopenSource = [&] {
        if (!sourceWasOpen) return;
        QMutexLocker locker(&m_mutex);
        m_file.setFileName(m_filePath);
        m_file.open(QIODevice::ReadOnly);
    };

    QSaveFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly)) { reopenSource(); return false; }
    if (out.write(payload) != payload.size()) { reopenSource(); return false; }
    if (!out.commit())                        { reopenSource(); return false; }

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
                m_fileBacked  = true;
                m_materialized = false;
            } else {
                m_data        = payload;
                m_fileBacked  = false;
                m_materialized = true;
            }
        } else {
            m_data        = payload;
            m_fileBacked  = false;
            m_materialized = true;
        }

        m_originalHash = computeCurrentHashLocked();
    }
    return true;
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

    for (auto it = m_overrides.cbegin(); it != m_overrides.cend(); ++it) {
        const qint64 pos = it.key();
         if (pos >= 0 && pos < result.size())
            result[pos] = it.value();
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
    auto it = m_chunkLruIter.find(chunkIndex);
    if (it != m_chunkLruIter.end())
        m_chunkLruList.erase(it.value());
    
    m_chunkLruList.push_front(chunkIndex);
    m_chunkLruIter[chunkIndex] = m_chunkLruList.begin();
}

void FileDataBuffer::trimChunkCacheLocked() const
{
    while ((int)m_chunkLruList.size() > m_maxCachedChunks) {
        qint64 oldest = m_chunkLruList.back();
        m_chunkLruList.pop_back();
        m_chunkLruIter.remove(oldest);
        m_chunkCache.remove(oldest);
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
    m_chunkLruList.clear();
    m_chunkLruIter.clear();
    m_fileBacked = false;
    m_materialized = true;
    m_baseSize = m_data.size();
}

uint FileDataBuffer::computeCurrentHashLocked() const
{
    if (m_materialized)
        return qHash(m_data, 0);

    uint h = 0;
    qint64 offset = 0;
    while (offset < m_baseSize) {
        const qint64 len = qMin<qint64>(m_chunkSize, m_baseSize - offset);
        h ^= qHash(readLocked(offset, len), (uint)offset);
        offset += len;
    }
    return h;
}
