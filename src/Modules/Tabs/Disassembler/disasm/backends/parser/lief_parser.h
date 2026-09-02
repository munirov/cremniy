#ifndef LIEF_PARSER_H
#define LIEF_PARSER_H

#include <QVector>
#include <QString>
#include <capstone/capstone.h>

struct ParsedBinary {
    struct Section {
        QString name;
        std::vector<uint8_t> bytes;
        quint64 vaddr = 0;
        quint64 fileOffset = 0;
    };

    QVector<Section> sections;
    cs_arch arch = CS_ARCH_ALL;
    cs_mode mode = CS_MODE_LITTLE_ENDIAN;
    QString error;

    [[nodiscard]] bool ok() const { return error.isEmpty(); }
};

class LiefParser
{
public:
    static ParsedBinary parse(const QString &filePath);
};

#endif // LIEF_PARSER_H