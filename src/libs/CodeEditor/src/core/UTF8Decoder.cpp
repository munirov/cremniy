#include "UTF8Decoder.h"
#include <QDebug>

UTF8Decoder::UTF8Decoder() {
}

QString UTF8Decoder::decode(const QByteArray& bytes) const {
    QString result;
    result.reserve(bytes.size()); // Reserve space for efficiency
    
    qint64 i = 0;
    while (i < bytes.size()) {
        unsigned char b = static_cast<unsigned char>(bytes[i]);
        
        // Determine sequence length
        int len = utf8SequenceLength(b);
        
        if (len == 0) {
            // Invalid first byte
            result.append(QChar(kReplacementChar));
            i++;
            continue;
        }
        
        // Check if we have enough bytes
        if (i + len > bytes.size()) {
            // Incomplete sequence at end of buffer
            result.append(QChar(kReplacementChar));
            i++;
            continue;
        }
        
        // Validate continuation bytes
        if (!validateContinuationBytes(bytes, i, len)) {
            // Invalid continuation bytes
            result.append(QChar(kReplacementChar));
            i++;
            continue;
        }
        
        // Decode the sequence
        if (len == 1) {
            // ASCII
            result.append(QChar(b));
        } else if (len == 2) {
            // 2-byte sequence
            uint32_t codePoint = ((b & 0x1F) << 6) | 
                                 (static_cast<unsigned char>(bytes[i + 1]) & 0x3F);
            result.append(QChar(codePoint));
        } else if (len == 3) {
            // 3-byte sequence
            uint32_t codePoint = ((b & 0x0F) << 12) | 
                                 ((static_cast<unsigned char>(bytes[i + 1]) & 0x3F) << 6) |
                                 (static_cast<unsigned char>(bytes[i + 2]) & 0x3F);
            result.append(QChar(codePoint));
        } else if (len == 4) {
            // 4-byte sequence (surrogate pair)
            uint32_t codePoint = ((b & 0x07) << 18) | 
                                 ((static_cast<unsigned char>(bytes[i + 1]) & 0x3F) << 12) |
                                 ((static_cast<unsigned char>(bytes[i + 2]) & 0x3F) << 6) |
                                 (static_cast<unsigned char>(bytes[i + 3]) & 0x3F);
            
            // Convert to UTF-16 surrogate pair
            if (codePoint >= 0x10000 && codePoint <= 0x10FFFF) {
                codePoint -= 0x10000;
                result.append(QChar(0xD800 + (codePoint >> 10)));
                result.append(QChar(0xDC00 + (codePoint & 0x3FF)));
            } else {
                result.append(QChar(kReplacementChar));
            }
        }
        
        i += len;
    }
    
    return result;
}

QString UTF8Decoder::decodeWithBOM(const QByteArray& bytes, bool& hasBOM) const {
    hasBOM = false;
    
    // Check for UTF-8 BOM
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == kUtf8Bom[0] &&
        static_cast<unsigned char>(bytes[1]) == kUtf8Bom[1] &&
        static_cast<unsigned char>(bytes[2]) == kUtf8Bom[2]) {
        hasBOM = true;
        // Decode without BOM
        return decode(bytes.mid(3));
    }
    
    return decode(bytes);
}

qint64 UTF8Decoder::findCharBoundary(const QByteArray& bytes, qint64 pos) const {
    if (pos <= 0) return 0;
    if (pos >= bytes.size()) return bytes.size();
    
    // Move backwards to find the start of a UTF-8 sequence
    qint64 boundary = pos;
    while (boundary > 0 && isUTF8Continuation(static_cast<unsigned char>(bytes[boundary]))) {
        boundary--;
    }
    
    return boundary;
}

qint64 UTF8Decoder::charPosToByte(const QString& text, qint64 charPos) const {
    if (charPos <= 0) return 0;
    if (charPos >= text.length()) {
        return text.toUtf8().size();
    }
    
    // Convert substring to UTF-8 and get its size
    QString substring = text.left(charPos);
    return substring.toUtf8().size();
}

qint64 UTF8Decoder::charPosToByte(const QByteArray& bytes, qint64 charPos) const {
    if (charPos <= 0 || bytes.isEmpty()) return 0;
    
    qint64 byteIndex = 0;
    qint64 currentCharPos = 0;
    
    while (byteIndex < bytes.size() && currentCharPos < charPos) {
        unsigned char b = static_cast<unsigned char>(bytes[byteIndex]);
        int len = utf8SequenceLength(b);
        
        if (len == 0 || byteIndex + len > bytes.size() || 
            !validateContinuationBytes(bytes, byteIndex, len)) {
            // Invalid sequence decodes to 1 replacement character
            byteIndex++;
            currentCharPos++;
        } else {
            // Valid sequence
            if (len == 4) {
                uint32_t codePoint = ((b & 0x07) << 18) | 
                                     ((static_cast<unsigned char>(bytes[byteIndex + 1]) & 0x3F) << 12) |
                                     ((static_cast<unsigned char>(bytes[byteIndex + 2]) & 0x3F) << 6) |
                                     (static_cast<unsigned char>(bytes[byteIndex + 3]) & 0x3F);
                if (codePoint >= 0x10000 && codePoint <= 0x10FFFF) {
                    currentCharPos += 2; // Surrogate pair adds 2 QChars
                } else {
                    currentCharPos++;
                }
            } else {
                currentCharPos++;
            }
            byteIndex += len;
        }
    }
    
    return byteIndex;
}

qint64 UTF8Decoder::byteToCharPos(const QByteArray& bytes, qint64 bytePos) const {
    if (bytePos <= 0) return 0;
    if (bytePos >= bytes.size()) {
        return decode(bytes).length();
    }
    
    // Decode bytes up to position and count characters
    QByteArray substring = bytes.left(bytePos);
    return decode(substring).length();
}

bool UTF8Decoder::isUTF8Continuation(unsigned char byte) const {
    return (byte & 0xC0) == 0x80;
}

int UTF8Decoder::utf8SequenceLength(unsigned char firstByte) const {
    if ((firstByte & 0x80) == 0) {
        // 0xxxxxxx - ASCII
        return 1;
    } else if ((firstByte & 0xE0) == 0xC0) {
        // 110xxxxx - 2-byte sequence
        return 2;
    } else if ((firstByte & 0xF0) == 0xE0) {
        // 1110xxxx - 3-byte sequence
        return 3;
    } else if ((firstByte & 0xF8) == 0xF0) {
        // 11110xxx - 4-byte sequence
        return 4;
    }
    
    // Invalid first byte
    return 0;
}

bool UTF8Decoder::validateContinuationBytes(const QByteArray& bytes, qint64 start, int length) const {
    for (int j = 1; j < length; j++) {
        if (!isUTF8Continuation(static_cast<unsigned char>(bytes[start + j]))) {
            return false;
        }
    }
    return true;
}
