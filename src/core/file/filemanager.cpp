#include "filemanager.h"
#include <qdir.h>
#include <qfileinfo.h>
#include <QJsonObject>


void FileManager::saveFile(FileContext* fc, const QByteArray* data){
    QFileInfo info(fc->filePath());
    QDir dir = info.dir();

    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile f(fc->filePath());
    if (!f.open(QFile::WriteOnly)) {
        qDebug() << "File open failed:" << f.errorString();
        return;
    }

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

void FileManager::saveJson(FileContext &fc, const QJsonObject &json) {
    QFile f(fc.filePath());

    if (!f.exists()) QDir().mkpath(QFileInfo(fc.filePath()).absolutePath());

    if (!f.open(QFile::WriteOnly)) {
        return;
    }

    const QJsonDocument doc(json);
    f.write(doc.toJson());
    f.close();
}

QJsonObject FileManager::loadJson(FileContext &fc) {
    QFile file(fc.filePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
        return {};

    const QJsonObject root = doc.object();

    file.close();
    return root;
}
