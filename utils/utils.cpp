#include "utils.h"

#include <QByteArray>
#include <QtGlobal>

bool isBinary(const QByteArray &data) {
    int nonPrintable = 0;
    int checked = qMin(data.size(), 4096);

    for (int i = 0; i < checked; ++i) {
        unsigned char c = data[i];

        if (c == 0)
            return true;

        if (c < 32 && c != '\n' && c != '\r' && c != '\t')
            nonPrintable++;
    }

    return (double)nonPrintable / checked > 0.3;
}