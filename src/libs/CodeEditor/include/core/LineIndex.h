#ifndef LINEINDEX_H
#define LINEINDEX_H

#include <QVector>
#include <qglobal.h>

class FileDataBuffer;

/**
 * @brief Line index for fast line-to-byte position mapping
 * 
 * Maintains an index of line positions in a buffer for quick conversion
 * between line numbers and byte offsets. For large files (>10000 lines),
 * uses a sampled index to reduce memory usage.
 */
class LineIndex {
public:
    LineIndex();
    
    /**
     * @brief Build the line index from a buffer
     * @param buffer The FileDataBuffer to index
     * 
     * Scans the buffer for newline characters (\n, \r\n, \r) and builds
     * an index mapping line numbers to byte offsets.
     */
    void build(FileDataBuffer* buffer);
    
    /**
     * @brief Incrementally update the index after buffer changes
     * @param bytePos Position where change occurred
     * @param bytesAdded Number of bytes added
     * @param bytesRemoved Number of bytes removed
     * 
     * Updates the line index without full rebuild when possible.
     */
    void update(qint64 bytePos, qint64 bytesAdded, qint64 bytesRemoved);
    
    /**
     * @brief Get the byte offset of the start of a line
     * @param lineNum Line number (0-indexed)
     * @return Byte offset of line start, or -1 if invalid
     */
    qint64 lineStartPos(qint64 lineNum) const;
    
    /**
     * @brief Get the length of a line in bytes
     * @param lineNum Line number (0-indexed)
     * @return Length in bytes (including newline), or 0 if invalid
     */
    qint64 lineLength(qint64 lineNum) const;
    
    /**
     * @brief Get the line number containing a byte position
     * @param bytePos Byte position in buffer
     * @return Line number (0-indexed), or -1 if invalid
     */
    qint64 lineFromBytePos(qint64 bytePos) const;
    
    /**
     * @brief Get the total number of lines
     * @return Total line count
     */
    qint64 lineCount() const;
    
    /**
     * @brief Clear the index
     */
    void clear();

private:
    /**
     * @brief Information about a single line
     */
    struct LineInfo {
        qint64 byteOffset;  // Start position in buffer
        qint64 byteLength;  // Length including newline
        
        LineInfo() : byteOffset(0), byteLength(0) {}
        LineInfo(qint64 offset, qint64 length) 
            : byteOffset(offset), byteLength(length) {}
    };
    
    QVector<LineInfo> m_lines;
    
    // For large files: sampled index
    static constexpr qint64 kSampleInterval = 1000;
    static constexpr qint64 kLargeFileThreshold = 10000;
    QVector<LineInfo> m_sampledLines;
    bool m_useSampling;
    
    /**
     * @brief Linear search for line containing byte position
     * @param bytePos Byte position to search for
     * @param startLine Line to start searching from
     * @return Line number containing bytePos
     */
    qint64 findLineLinearSearch(qint64 bytePos, qint64 startLine) const;
    
    /**
     * @brief Build sampled index from full index
     */
    void buildSampledIndex();
};

#endif // LINEINDEX_H
