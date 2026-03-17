#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include <qobject.h>
class FileManager
{
public:
    // Action methods
    static void saveFile(QByteArray* data);
    static QByteArray* openFile();


};

#endif // FILEMANAGER_H
