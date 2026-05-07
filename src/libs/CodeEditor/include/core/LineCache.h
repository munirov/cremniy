#ifndef LINECACHE_H
#define LINECACHE_H

#include <QHash>
#include <QList>
#include <QString>
#include <qglobal.h>

/**
 * @brief LRU cache for decoded text lines
 * 
 * Caches decoded QString lines to avoid repeated UTF-8 decoding.
 * Uses Least Recently Used (LRU) eviction policy to stay within
 * memory limits (default 50 MB).
 */
class LineCache {
public:
    /**
     * @brief Constructor
     * @param maxMemoryBytes Maximum memory usage in bytes (default 50 MB)
     */
    explicit LineCache(qint64 maxMemoryBytes = 50 * 1024 * 1024);
    
    /**
     * @brief Get a cached line
     * @param lineNum Line number (0-indexed)
     * @return Pointer to cached QString, or nullptr if not cached
     */
    QString* get(qint64 lineNum);
    
    /**
     * @brief Add a line to the cache
     * @param lineNum Line number (0-indexed)
     * @param text The decoded line text
     */
    void put(qint64 lineNum, const QString& text);
    
    /**
     * @brief Invalidate a range of lines
     * @param startLine First line to invalidate (inclusive)
     * @param endLine Last line to invalidate (inclusive)
     */
    void invalidate(qint64 startLine, qint64 endLine);
    
    /**
     * @brief Clear all cached lines
     */
    void clear();
    
    /**
     * @brief Get current memory usage
     * @return Memory usage in bytes
     */
    qint64 memoryUsage() const;
    
    /**
     * @brief Set maximum memory limit
     * @param bytes Maximum memory in bytes
     */
    void setMaxMemory(qint64 bytes);
    
    /**
     * @brief Get maximum memory limit
     * @return Maximum memory in bytes
     */
    qint64 maxMemory() const;

private:
    /**
     * @brief Cache entry with metadata
     */
    struct CacheEntry {
        QString text;
        qint64 memorySize;
        qint64 lastAccessTime;
        
        CacheEntry() : memorySize(0), lastAccessTime(0) {}
        CacheEntry(const QString& t, qint64 size, qint64 time)
            : text(t), memorySize(size), lastAccessTime(time) {}
    };
    
    QHash<qint64, CacheEntry> m_cache;
    QList<qint64> m_lruList;
    qint64 m_maxMemoryBytes;
    qint64 m_currentMemoryBytes;
    qint64 m_accessCounter;
    
    /**
     * @brief Evict least recently used entries
     */
    void evictLRU();
    
    /**
     * @brief Update access time for a line
     * @param lineNum Line number to touch
     */
    void touch(qint64 lineNum);
    
    /**
     * @brief Calculate memory size of a QString
     * @param text The string to measure
     * @return Memory size in bytes
     */
    qint64 calculateMemorySize(const QString& text) const;
};

#endif // LINECACHE_H
