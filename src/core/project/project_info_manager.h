#ifndef PROJECT_INFO_MANAGER_H
#define PROJECT_INFO_MANAGER_H

#include <qobject.h>

#include <QString>
#include <QStringList>
#include <QDir>
#include <QJsonObject>

struct ProjectInfo {

    QString name;
    QString language;
    QString buildCommand = "";
    QString path;

    // сериализация из структуры в json
    QJsonObject toJson() const {
        QJsonObject json;
        json["name"] = name;
        json["language"] = language;
        json["buildCommand"] = buildCommand;
        json["path"] = path;
        return json;
    }

    // дессериализация из json в структуру
    static ProjectInfo fromJson(const QJsonObject &json) {
        ProjectInfo info;
        info.name = json["name"].toString();
        info.language = json["language"].toString();
        info.buildCommand = json["buildCommand"].toString();
        info.path = json["path"].toString();
        return info;
    }

};

class ProjectInfoManager {

    // используется для получения и сохранения информации о проекте в файле project.cremniy

    private:
        static inline const QString m_projectInfoFileName = "project.cremniy";
        static QString projectInfoFilePath(const QString &projectPath);

    public:
        static bool loadProjectInfo(const QString &projectPath, ProjectInfo &outInfo);
        static void saveProjectInfo(const ProjectInfo &projInfo);


};

#endif// PROJECT_INFO_MANAGER_H
