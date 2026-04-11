#include "QHexView/model/qhexdocument.h"
#include <QDataStream>
#include <QGlobalStatic>
#include <QHash>
#include <QHexView/model/qhexoptions.h>
#include <QHexView/model/qhexutils.h>
#include <QHexView/qhexview.h>
#include <QList>
#include <QtEndian>
#include <limits>

#if defined(_WIN32) && _MSC_VER <= 1916 // v141_xp
#include <ctype.h>
namespace std {
using ::tolower;
}
#else
#include <cctype>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define QHEXVIEW_VARIANT_EQ(x, t) ((x).metaType().id() == QMetaType::Q##t)
#else
#define QHEXVIEW_VARIANT_EQ(x, t) ((x).type() == QVariant::t)
#endif

namespace QHexUtils {

Q_GLOBAL_STATIC_WITH_ARGS(QList<char>, HEXMAP,
                          ({'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'a', 'b', 'c', 'd', 'e', 'f'}));

bool isHex(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') ||
           (ch >= 'a' && ch <= 'f');
}

namespace PatternUtils {

bool match(const QHexDocument* doc, const QHexPattern& pattern, qint64 idx,
           qint64& matchlen) {
    if(doc->isEmpty() || pattern.isEmpty())
        return false;

    int ppos = 0, dpos = idx;
    int skipidx = -1, matchidx = -1;
    qint64 len = doc->length();

    while(ppos < pattern.size()) {
        if(dpos >= len)
            return false;

        if((pattern[ppos].type == QHexPatternType::WILDCARD) ||
           (pattern[ppos].type == QHexPatternType::BYTE &&
            pattern[ppos].b == doc->at(dpos))) {
            dpos++;
            ppos++;
        }
        else if(pattern[ppos].type == QHexPatternType::SKIP) {
            skipidx = ppos++;
            matchidx = dpos;
        }
        else if(skipidx != -1) {
            ppos = skipidx + 1;
            dpos = ++matchidx;
        }
        else
            return false;
    }

    while(ppos < pattern.size() && pattern[ppos].type == QHexPatternType::SKIP)
        ppos++;

    matchlen = dpos - idx;
    return ppos == pattern.size();
}

bool nextChar(const QString& s, int& i, char& ch) {
    while(i < s.size() && s[i].isSpace()) {
        ch = ' ';
        i++;
    }

    if(i >= s.size())
        return false;

    ch = s[i++].cell();
    return true;
}

quint8 mkHex_1(char c) {
    if('0' <= c && c <= '9')
        return c - '0';
    if('a' <= c && c <= 'f')
        return c - 'a' + 10;
    if('A' <= c && c <= 'F')
        return c - 'A' + 10;

    qFatal("Invalid hex integer '%c'", c);
    return 0;
}

quint8 mkHex(char b1, char b2) {
    quint8 h = PatternUtils::mkHex_1(b1);
    quint8 l = PatternUtils::mkHex_1(b2);
    return (h << 4) | l;
}

QHexPattern compile(const QString& s) {
    QHexPattern p;
    char b1 = 0, b2 = 0;

    for(int i = 0; i < s.length();) {
        if(!PatternUtils::nextChar(s, i, b1) ||
           !PatternUtils::nextChar(s, i, b2)) {
            if(std::isspace(b1))
                break;
            return {};
        }

        if(b1 == '?' && b2 == b1)
            p.push_back({QHexPatternType::WILDCARD, {}});
        else if(b1 == '.' && b2 == b1)
            p.push_back({QHexPatternType::SKIP, {}});
        else if(QHexUtils::isHex(b1) && QHexUtils::isHex(b2))
            p.push_back({QHexPatternType::BYTE, PatternUtils::mkHex(b1, b2)});
    }

    return p;
}

} // namespace PatternUtils

namespace {

unsigned int countBits(uint val) {
    if(val <= std::numeric_limits<quint8>::max())
        return QHexFindOptions::Int8;
    if(val <= std::numeric_limits<quint16>::max())
        return QHexFindOptions::Int16;
    if(val <= std::numeric_limits<quint32>::max())
        return QHexFindOptions::Int32;

    return QHexFindOptions::Int64;
}

template<typename Function>
qint64 findIter(qint64 startoffset, QHexFindDirection fd,
                const QHexView* hexview, Function&& f) {
    QHexDocument* hexdocument = hexview->hexDocument();
    qint64 offset = -1;

    QHexFindDirection cfd = fd;
    if(cfd == QHexFindDirection::All)
        cfd = QHexFindDirection::Forward;

    qint64 i = startoffset;

    bool restartLoopOnce = true;

    while(offset == -1 &&
          (cfd == QHexFindDirection::Backward ? (i >= 0)
                                              : (i < hexdocument->length()))) {
        if(!f(i, offset))
            break;

        if(cfd == QHexFindDirection::Backward)
            i--;
        else
            i++;

        if(fd == QHexFindDirection::All && i >= hexdocument->length() &&
           restartLoopOnce) {
            i = 0;
            restartLoopOnce = false;
        }
    }

    return offset;
}

qint64 findDefault(const QByteArray& value, qint64 startoffset,
                   const QHexView* hexview, unsigned int options,
                   QHexFindDirection fd) {
    QHexDocument* hexdocument = hexview->hexDocument();
    if(value.size() > hexdocument->length())
        return -1;

    return QHexUtils::findIter(
        startoffset, fd, hexview,
        [options, value, hexdocument](qint64 idx, qint64& offset) -> bool {
            for(auto i = 0; i < value.size(); i++) {
                qint64 curroffset = idx + i;

                if(curroffset >= hexdocument->length()) {
                    offset = -1;
                    return false;
                }

                uchar ch1 = hexdocument->at(curroffset);
                uchar ch2 = value.at(i);

                if(!(options & QHexFindOptions::CaseSensitive)) {
                    ch1 = std::tolower(ch1);
                    ch2 = std::tolower(ch2);
                }

                if(ch1 != ch2)
                    break;
                if(i == value.size() - 1)
                    offset = idx;
            }

            return true;
        });
}

qint64 findWildcard(QString pattern, qint64 startoffset,
                    const QHexView* hexview, QHexFindDirection fd,
                    qint64& matchlen) {
    QHexDocument* hexdocument = hexview->hexDocument();
    QHexPattern p = PatternUtils::compile(pattern);

    return findIter(startoffset, fd, hexview,
                    [&](qint64 idx, qint64& offset) -> bool {
                        if(PatternUtils::match(hexdocument, p, idx, matchlen))
                            offset = idx;
                        return true;
                    });
}

QByteArray variantToByteArray(QVariant value, QHexFindMode mode,
                              unsigned int options) {
    QByteArray v;

    switch(mode) {
        case QHexFindMode::Text:
            if(QHEXVIEW_VARIANT_EQ(value, String))
                v = value.toString().toUtf8();
            else if(QHEXVIEW_VARIANT_EQ(value, ByteArray))
                v = value.toByteArray();
            break;

        case QHexFindMode::Hex: {
            if(QHEXVIEW_VARIANT_EQ(value, String)) {
                qint64 len = 0;
                auto s = value.toString();
                if(!QHexUtils::checkPattern(s))
                    return {};

                bool ok = true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                for(auto i = 0; ok && i < s.size(); i += 2)
                    v.push_back(static_cast<char>(
                        QStringView{s}.mid(i, 2).toUInt(&ok, 16)));
#else
                for(auto i = 0; ok && i < s.size(); i += 2)
                    v.push_back(
                        static_cast<char>(s.midRef(i, 2).toUInt(&ok, 16)));
#endif

                if(!ok)
                    return {};
            }
            else if(QHEXVIEW_VARIANT_EQ(value, ByteArray))
                v = value.toByteArray();
            break;
        }

        case QHexFindMode::Int: {
            bool ok = false;
            uint val = value.toUInt(&ok);
            if(!ok)
                return QByteArray{};

            QDataStream ds(&v, QIODevice::WriteOnly);

            if(options & QHexFindOptions::BigEndian) {
                if(options & QHexFindOptions::Int8)
                    ds << qToBigEndian<quint8>(val);
                else if(options & QHexFindOptions::Int16)
                    ds << qToBigEndian<quint16>(val);
                else if(options & QHexFindOptions::Int32)
                    ds << qToBigEndian<quint32>(val);
                else if(options & QHexFindOptions::Int64)
                    ds << qToBigEndian<quint64>(val);
                else
                    return variantToByteArray(value, mode,
                                              options | countBits(val));
            }
            else {
                if(options & QHexFindOptions::Int8)
                    ds << static_cast<quint8>(val);
                else if(options & QHexFindOptions::Int16)
                    ds << static_cast<quint16>(val);
                else if(options & QHexFindOptions::Int32)
                    ds << static_cast<quint32>(val);
                else if(options & QHexFindOptions::Int64)
                    ds << static_cast<quint64>(val);
                else
                    return variantToByteArray(value, mode,
                                              options | countBits(val));
            }

            break;
        }

        case QHexFindMode::Float: {
            bool ok = false;
            QDataStream ds(&v, QIODevice::WriteOnly);
            if(options & QHexFindOptions::Float)
                ds << value.toFloat(&ok);
            else if(options & QHexFindOptions::Double)
                ds << value.toDouble(&ok);
            if(!ok)
                return {};
        }

        default: break;
    }

    return v;
}

} // namespace

QByteArray toHex(quint8 b) {
    QByteArray hex(2, Qt::Uninitialized);
    hex[0] = HEXMAP->at((b & 0xf0) >> 4);
    hex[1] = HEXMAP->at(b & 0x0f);
    return hex;
}

QByteArray toHex(const QByteArray& ba, char sep) {
    if(ba.isEmpty()) {
        return QByteArray();
    }

    QByteArray hex(sep ? (ba.size() * 3 - 1) : (ba.size() * 2),
                   Qt::Uninitialized);

    for(auto i = 0, o = 0; i < ba.size(); i++) {
        if(sep && i)
            hex[o++] = static_cast<uchar>(sep);
        hex[o++] = HEXMAP->at((ba.at(i) & 0xf0) >> 4);
        hex[o++] = HEXMAP->at(ba.at(i) & 0x0f);
    }

    return hex;
}

QByteArray toHex(const QByteArray& ba) { return QHexUtils::toHex(ba, '\0'); }

qint64 adjustColumn(const QHexOptions* options, qint64 col) {
    if(!options->hasFlag(QHexFlags::InvertedByteOrder) ||
       options->group_length <= 1)
        return col;

    qint64 grpstart = (col / options->group_length) * options->group_length;
    qint64 grpoff = col % options->group_length;
    qint64 swpoff = options->group_length - 1 - grpoff;
    return grpstart + swpoff;
}

qint64 positionToOffset(const QHexOptions* options, QHexPosition pos) {
    return options->line_length * pos.line +
           QHexUtils::adjustColumn(options, pos.column);
}

QHexPosition offsetToPosition(const QHexOptions* options, qint64 offset) {
    return {offset / options->line_length, offset % options->line_length};
}

bool checkPattern(const QString& s) {
    return !PatternUtils::compile(s).isEmpty();
}

QPair<qint64, qint64> find(const QHexView* hexview, QVariant value,
                           qint64 startoffset, QHexFindMode mode,
                           unsigned int options, QHexFindDirection fd) {
    qint64 offset = -1, size = 0;
    if(startoffset == -1)
        startoffset = static_cast<qint64>(hexview->offset());

    if(mode == QHexFindMode::Hex && QHEXVIEW_VARIANT_EQ(value, String)) {
        offset = QHexUtils::findWildcard(value.toString(), startoffset, hexview,
                                         fd, size);
    }
    else {
        auto ba = variantToByteArray(value, mode, options);

        if(!ba.isEmpty()) {
            offset =
                QHexUtils::findDefault(ba, startoffset, hexview, options, fd);
            size = ba.size();
        }
        else
            offset = -1;
    }

    return {offset, offset > -1 ? size : 0};
}

QPair<qint64, qint64> replace(const QHexView* hexview, QVariant oldvalue,
                              QVariant newvalue, qint64 startoffset,
                              QHexFindMode mode, unsigned int options,
                              QHexFindDirection fd) {
    auto res =
        QHexUtils::find(hexview, oldvalue, startoffset, mode, options, fd);

    if(res.first != -1 && res.second > 0) {
        QHexDocument* hexdocument = hexview->hexDocument();
        auto ba = variantToByteArray(newvalue, mode, options);

        if(!ba.isEmpty()) {
            hexdocument->remove(res.first, res.second);
            hexdocument->insert(res.first, ba);
            res.second = ba.size();
        }
        else {
            res.first = -1;
            res.second = 0;
        }
    }

    return res;
}

} // namespace QHexUtils
