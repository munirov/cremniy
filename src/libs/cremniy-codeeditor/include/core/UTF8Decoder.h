#ifndef UTF8DECODER_H
#define UTF8DECODER_H

#include <QByteArray>
#include <QString>

/**
 * @brief UTF-8 decoder with error handling
 * 
 * Decodes UTF-8 byte sequences to Unicode strings, handling invalid
 * sequences by replacing them with the Unicode replacement character (U+FFFD).
 * Supports 1-byte (ASCII), 2-byte, 3-byte, and 4-byte UTF-8 sequences.
 */
class UTF8Decoder {
public:
    UTF8Decoder();
    
    /**
     * @brief Decode UTF-8 bytes to QString
     * @param bytes The byte array to decode
     * @return Decoded QString with replacement characters for invalid sequences
     */
    QString decode(const QByteArray& bytes) const;
    
    /**
     * @brief Decode UTF-8 bytes with BOM detection
     * @param bytes The byte array to decode
     * @param hasBOM Output parameter indicating if BOM was found
     * @return Decoded QString (BOM is removed from output)
     */
    QString decodeWithBOM(const QByteArray& bytes, bool& hasBOM) const;
    
    /**
     * @brief Find the nearest UTF-8 character boundary
     * @param bytes The byte array to search
     * @param pos Position to start searching from
     * @return Position of the character boundary (start of UTF-8 sequence)
     */
    qint64 findCharBoundary(const QByteArray& bytes, qint64 pos) const;
    
    /**
     * @brief Convert character position to byte position using original bytes
     * @param bytes The raw original byte array
     * @param charPos Character position (0-indexed)
     * @return Byte position in the raw byte array
     */
    qint64 charPosToByte(const QByteArray& bytes, qint64 charPos) const;

    /**
     * @brief Convert character position to byte position
     * @param text The text string
     * @param charPos Character position (0-indexed)
     * @return Byte position in UTF-8 encoding
     */
    qint64 charPosToByte(const QString& text, qint64 charPos) const;
    
    /**
     * @brief Convert byte position to character position
     * @param bytes The UTF-8 byte array
     * @param bytePos Byte position (0-indexed)
     * @return Character position in decoded string
     */
    qint64 byteToCharPos(const QByteArray& bytes, qint64 bytePos) const;

private:
    static constexpr char32_t kReplacementChar = 0xFFFD;
    static constexpr unsigned char kUtf8Bom[3] = {0xEF, 0xBB, 0xBF};
    
    /**
     * @brief Check if byte is a UTF-8 continuation byte
     * @param byte The byte to check
     * @return true if byte is in range 0x80-0xBF
     */
    bool isUTF8Continuation(unsigned char byte) const;
    
    /**
     * @brief Get the length of UTF-8 sequence from first byte
     * @param firstByte The first byte of the sequence
     * @return Sequence length (1-4), or 0 if invalid
     */
    int utf8SequenceLength(unsigned char firstByte) const;
    
    /**
     * @brief Validate UTF-8 continuation bytes
     * @param bytes The byte array
     * @param start Start position
     * @param length Expected sequence length
     * @return true if all continuation bytes are valid
     */
    bool validateContinuationBytes(const QByteArray& bytes, qint64 start, int length) const;
};

#endif // UTF8DECODER_H
