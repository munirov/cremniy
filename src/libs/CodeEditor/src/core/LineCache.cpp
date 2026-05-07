#include "LineCache.h"
#include <QDebug>
#include <algorithm>
#include <new>

LineCache::LineCache(qint64 maxMemoryBytes)
    : m_maxMemoryBytes(maxMemoryBytes)
    , m_currentMemoryBytes(0)
    , m_accessCounter(0) {
}

QString* LineCache::get(qint64 lineNum) {
    auto it = m_cache.find(lineNum);
    if (it == m_cache.end()) {
        // Cache miss
        return nullptr;
    }
    
    // Cache hit - update access time
    touch(lineNum);
    return &it.value().text;
}

void LineCache::put(qint64 lineNum, const QString& text) {
    try {
        qint64 memSize = calculateMemorySize(text);
        if (memSize > m_maxMemoryBytes)
            return;

        auto it = m_cache.find(lineNum);
        if (it != m_cache.end()) {
            m_currentMemoryBytes -= it.value().memorySize;
            m_currentMemoryBytes += memSize;
            it.value().text = text;
            it.value().memorySize = memSize;
            touch(lineNum);
        } else {
            m_currentMemoryBytes += memSize;
            m_cache.insert(lineNum, CacheEntry(text, memSize, ++m_accessCounter));
            m_lruList.append(lineNum);
            if (m_currentMemoryBytes > m_maxMemoryBytes)
                evictLRU();
        }
    } catch (const std::bad_alloc&) {
        qWarning() << "LineCache allocation failed, clearing cache";
        clear();
    }
}

void LineCache::invalidate(qint64 startLine, qint64 endLine) {
    QList<qint64> toRemove;
    
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        qint64 lineNum = it.key();
        if (lineNum >= startLine && lineNum <= endLine) {
            toRemove.append(lineNum);
        }
    }
    
    for (qint64 lineNum : toRemove) {
        auto it = m_cache.find(lineNum);
        if (it != m_cache.end()) {
            m_currentMemoryBytes -= it.value().memorySize;
            m_cache.erase(it);
            m_lruList.removeOne(lineNum);
        }
    }
}

void LineCache::clear() {
    m_cache.clear();
    m_lruList.clear();
    m_currentMemoryBytes = 0;
    m_accessCounter = 0;
}

qint64 LineCache::memoryUsage() const {
    return m_currentMemoryBytes;
}

void LineCache::setMaxMemory(qint64 bytes) {
    static constexpr qint64 kHardLimitBytes = 50 * 1024 * 1024;
    m_maxMemoryBytes = qBound<qint64>(0, bytes, kHardLimitBytes);
    
    // Evict if current usage exceeds new limit
    if (m_currentMemoryBytes > m_maxMemoryBytes) {
        evictLRU();
    }
}

qint64 LineCache::maxMemory() const {
    return m_maxMemoryBytes;
}

void LineCache::evictLRU() {
    // Sort entries by access time
    QList<QPair<qint64, qint64>> entries; // (lineNum, accessTime)
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        entries.append(qMakePair(it.key(), it.value().lastAccessTime));
    }
    
    std::sort(entries.begin(), entries.end(), 
              [](const QPair<qint64, qint64>& a, const QPair<qint64, qint64>& b) {
                  return a.second < b.second; // Sort by access time (oldest first)
              });
    
    // Remove oldest entries until under limit
    for (const auto& entry : entries) {
        if (m_currentMemoryBytes <= m_maxMemoryBytes) {
            break;
        }
        
        qint64 lineNum = entry.first;
        auto it = m_cache.find(lineNum);
        if (it != m_cache.end()) {
            m_currentMemoryBytes -= it.value().memorySize;
            m_cache.erase(it);
            m_lruList.removeOne(lineNum);
        }
    }
    
    // Additional safety: limit total number of entries
    static constexpr int kMaxEntries = 1000;
    while (m_cache.size() > kMaxEntries) {
        if (!entries.isEmpty()) {
            qint64 lineNum = entries.first().first;
            auto it = m_cache.find(lineNum);
            if (it != m_cache.end()) {
                m_currentMemoryBytes -= it.value().memorySize;
                m_cache.erase(it);
                m_lruList.removeOne(lineNum);
            }
            entries.removeFirst();
        } else {
            break;
        }
    }
}

void LineCache::touch(qint64 lineNum) {
    auto it = m_cache.find(lineNum);
    if (it != m_cache.end()) {
        it.value().lastAccessTime = ++m_accessCounter;
        
        // Move to end of LRU list
        m_lruList.removeOne(lineNum);
        m_lruList.append(lineNum);
    }
}

qint64 LineCache::calculateMemorySize(const QString& text) const {
    // QString uses UTF-16 internally (2 bytes per character)
    // Plus overhead for QString object
    return text.length() * 2 + sizeof(QString);
}
