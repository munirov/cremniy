#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "filecontext.h"
#include <qobject.h>

class FileManager
{
public:

    static void saveFile(FileContext* fc, const QByteArray* data);
    static QByteArray openFile(FileContext* fc);

    static void saveJson(FileContext & fc, const QJsonObject& json);
    static QJsonObject loadJson(FileContext & fc);

};

#endif // FILEMANAGER_H
