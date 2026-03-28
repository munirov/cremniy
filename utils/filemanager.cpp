#include "filemanager.h"
#include <qfileinfo.h>


void FileManager::saveFile(FileContext* fc, QByteArray* data){
    QFile f(fc->filePath());
    if (!f.open(QFile::WriteOnly)) return;
    f.write(*data);
    f.close();
}

QByteArray FileManager::openFile(FileContext* fc){
    QFile file(fc->filePath());
    if (!file.open(QIODevice::ReadOnly)) return nullptr;
    QByteArray data = file.readAll();
    file.close();
    fc->m_bytesCount = data.size();
    fc->m_startOffset = 0;
    fc->m_endOffset = data.size() - 1;
    return data;
}