#ifndef SEARCHDEFS_H
#define SEARCHDEFS_H

#include <QString>
#include <QMetaType>

struct SearchResult {
    QString filePath;
    int lineNumber = 0;
    QString lineText;
    int matchStart = 0;
    int matchLength = 0;
    int matchCountOnLine = 0;
};

Q_DECLARE_METATYPE(SearchResult)

#endif // SEARCHDEFS_H
