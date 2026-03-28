#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "filecontext.h"
#include <qobject.h>
class FileManager
{
public:

    static void saveFile(FileContext* fc, QByteArray* data);
    static QByteArray openFile(FileContext* fc);

};

#endif // FILEMANAGER_H
