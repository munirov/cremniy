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

public:
    static QStringList loadProjectsHistory();
    static void saveProjectsHistory(const QString & projectsHistory);
};

} // utils

#endif //CREMNIY_PROJECTS_HISTORY_MANAGER_H
