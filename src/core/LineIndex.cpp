#include "LineIndex.h"
#include "FileDataBuffer.h"
#include <QDebug>
#include <algorithm>

LineIndex::LineIndex() 
    : m_useSampling(false) {
}

void LineIndex::build(FileDataBuffer* buffer) {
    clear();
    
    if (!buffer) {
        qWarning() << "LineIndex::build: null buffer";
        return;
    }
    
    qint64 bufferSize = buffer->size();
    if (bufferSize == 0) {
        // Empty buffer has one empty line
        m_lines.append(LineInfo(0, 0));
        return;
    }
    
    // Read buffer in chunks for efficiency
    static constexpr qint64 kChunkSize = 4096;
    qint64 currentOffset = 0;
    qint64 lineStart = 0;
    
    while (currentOffset < bufferSize) {
        qint64 chunkSize = qMin(kChunkSize, bufferSize - currentOffset);
        QByteArray chunk = buffer->read(currentOffset, chunkSize);
        
        for (qint64 i = 0; i < chunk.size(); i++) {
            char c = chunk[i];
            qint64 absolutePos = currentOffset + i;
            
            if (c == '\n') {
                // Unix newline
                qint64 lineLength = absolutePos - lineStart + 1;
                m_lines.append(LineInfo(lineStart, lineLength));
                lineStart = absolutePos + 1;
            } else if (c == '\r') {
                // Check for Windows newline (\r\n)
                if (i + 1 < chunk.size() && chunk[i + 1] == '\n') {
                    // Windows newline
                    qint64 lineLength = absolutePos - lineStart + 2;
                    m_lines.append(LineInfo(lineStart, lineLength));
                    lineStart = absolutePos + 2;
                    i++; // Skip the \n
                } else if (absolutePos + 1 < bufferSize) {
                    // Check next chunk for \n
                    char nextChar = buffer->getByte(absolutePos + 1);
                    if (nextChar == '\n') {
                        qint64 lineLength = absolutePos - lineStart + 2;
                        m_lines.append(LineInfo(lineStart, lineLength));
                        lineStart = absolutePos + 2;
                        i++; // Will be incremented again in loop
                    } else {
                        // Mac newline (just \r)
                        qint64 lineLength = absolutePos - lineStart + 1;
                        m_lines.append(LineInfo(lineStart, lineLength));
                        lineStart = absolutePos + 1;
                    }
                } else {
                    // Mac newline at end of file
                    qint64 lineLength = absolutePos - lineStart + 1;
                    m_lines.append(LineInfo(lineStart, lineLength));
                    lineStart = absolutePos + 1;
                }
            }
        }
        
        currentOffset += chunkSize;
    }
    
    // Handle last line if file doesn't end with newline
    if (lineStart < bufferSize) {
        qint64 lineLength = bufferSize - lineStart;
        m_lines.append(LineInfo(lineStart, lineLength));
    }
    
    // Build sampled index for large files
    if (m_lines.size() > kLargeFileThreshold) {
        buildSampledIndex();
        m_useSampling = true;
    }
    
    qDebug() << "LineIndex::build: indexed" << m_lines.size() << "lines, sampling:" << m_useSampling;
}

void LineIndex::update(qint64 bytePos, qint64 bytesAdded, qint64 bytesRemoved) {
    if (m_lines.isEmpty()) {
        return;
    }
    
    // Find affected line
    qint64 affectedLine = lineFromBytePos(bytePos);
    if (affectedLine < 0) {
        // Invalid position, rebuild
        qWarning() << "LineIndex::update: invalid position, rebuilding";
        return;
    }
    
    qint64 netChange = bytesAdded - bytesRemoved;
    
    // Update affected line length
    m_lines[affectedLine].byteLength += netChange;
    
    // Shift all subsequent lines
    for (qint64 i = affectedLine + 1; i < m_lines.size(); i++) {
        m_lines[i].byteOffset += netChange;
    }
    
    // Rebuild sampled index if using sampling
    if (m_useSampling) {
        buildSampledIndex();
    }
}

qint64 LineIndex::lineStartPos(qint64 lineNum) const {
    if (lineNum < 0 || lineNum >= m_lines.size()) {
        return -1;
    }
    return m_lines[lineNum].byteOffset;
}

qint64 LineIndex::lineLength(qint64 lineNum) const {
    if (lineNum < 0 || lineNum >= m_lines.size()) {
        return 0;
    }
    return m_lines[lineNum].byteLength;
}

qint64 LineIndex::lineFromBytePos(qint64 bytePos) const {
    if (m_lines.isEmpty()) {
        return -1;
    }
    
    if (bytePos < 0) {
        return 0;
    }
    
    if (m_useSampling) {
        // Binary search in sampled index
        qint64 left = 0;
        qint64 right = m_sampledLines.size() - 1;
        qint64 sampleLine = 0;
        
        while (left <= right) {
            qint64 mid = left + (right - left) / 2;
            if (m_sampledLines[mid].byteOffset <= bytePos) {
                sampleLine = mid;
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        
        // Linear search from sample point
        qint64 startLine = sampleLine * kSampleInterval;
        return findLineLinearSearch(bytePos, startLine);
    } else {
        // Binary search in full index
        qint64 left = 0;
        qint64 right = m_lines.size() - 1;
        qint64 result = 0;
        
        while (left <= right) {
            qint64 mid = left + (right - left) / 2;
            if (m_lines[mid].byteOffset <= bytePos) {
                result = mid;
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        
        return result;
    }
}

qint64 LineIndex::lineCount() const {
    return m_lines.size();
}

void LineIndex::clear() {
    m_lines.clear();
    m_sampledLines.clear();
    m_useSampling = false;
}

qint64 LineIndex::findLineLinearSearch(qint64 bytePos, qint64 startLine) const {
    for (qint64 i = startLine; i < m_lines.size(); i++) {
        qint64 lineEnd = m_lines[i].byteOffset + m_lines[i].byteLength;
        if (bytePos < lineEnd) {
            return i;
        }
    }
    return m_lines.size() - 1;
}

void LineIndex::buildSampledIndex() {
    m_sampledLines.clear();
    
    for (qint64 i = 0; i < m_lines.size(); i += kSampleInterval) {
        m_sampledLines.append(m_lines[i]);
    }
    
    qDebug() << "LineIndex::buildSampledIndex: created" << m_sampledLines.size() << "samples";
}
