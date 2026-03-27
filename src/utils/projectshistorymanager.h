//
// Created by Dmitriy on 3/27/26.
//

#ifndef CREMNIY_PROJECTS_HISTORY_MANAGER_H
#define CREMNIY_PROJECTS_HISTORY_MANAGER_H
#include <QStandardPaths>
#include <QString>

namespace utils {

class ProjectsHistoryManager {

private:
    static constexpr unsigned short maxLength = 15;

    static QString getDefaultPathLocation() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history_open_projects.dat";
    }

    ProjectsHistoryManager() = default;

    static void formatedDataRawAndSave(const QStringList & formatedList);
    static QStringList loadRawProjectsHistory();

public:
    static QStringList loadProjectsHistory();
    static void saveProjectsHistory(const QString & projectsHistory);
    static void checkDirectoryExists();
};

} // utils

#endif //CREMNIY_PROJECTS_HISTORY_MANAGER_H
