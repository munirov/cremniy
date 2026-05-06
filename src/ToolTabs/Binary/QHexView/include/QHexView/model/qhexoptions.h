#pragma once

#include <QBrush>
#include <QChar>
#include <QColor>
#include <QHash>

namespace QHexFlags {

// clang-format off
enum : unsigned int {
    None              = 1 << 0,
    HSeparator        = 1 << 1,
    VSeparator        = 1 << 2,
    StyledHeader      = 1 << 3,
    StyledAddress     = 1 << 4,
    NoHeader          = 1 << 5,
    HighlightAddress  = 1 << 6,
    HighlightColumn   = 1 << 7,
    PaddedAddress     = 1 << 8,
    PaddedHighlight   = 1 << 9,
    InvertedByteOrder = 1 << 10,

    Separators = HSeparator | VSeparator,
    Styled     = StyledHeader | StyledAddress,
};
// clang-format on

} // namespace QHexFlags

struct QHexCharFormat {
    QBrush background;
    QColor foreground;
    QColor underline;
};

struct QHexOptions {
    // Appearance
    QChar unprintable_char{'.'};
    QChar invalid_char{'?'};
    QString address_label{""};
    QString hex_label;
    QString ascii_label;
    quint64 base_address{0};
    unsigned int flags{QHexFlags::None};
    unsigned int line_length{0x10};
    unsigned int address_width{0};
    unsigned int group_length{1};
    int scroll_steps{1};

    // Colors & Styles
    QHash<quint8, QHexCharFormat> byte_colors;
    QColor linealt_background;
    QColor line_background;
    QHexCharFormat trackchange_format_insert;
    QHexCharFormat trackchange_format_overwrite;
    QHexCharFormat header_format;
    QHexCharFormat address_format;
    QHexCharFormat addressheader_format;
    QHexCharFormat hexheader_format;
    QHexCharFormat asciiheader_format;
    QColor comment_color;
    QColor separator_color;

    inline bool hasFlag(unsigned int flag) const { return flags & flag; }
};
