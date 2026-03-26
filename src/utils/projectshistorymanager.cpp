//
// Created by Dmitriy on 3/27/26.
//

#include "projectshistorymanager.h"

#include "filecontext.h"
#include "filemanager.h"

namespace utils {
    QStringList ProjectsHistoryManager::loadProjectsHistory() {
        FileContext fileContext(getDefaultPathLocation());
        const QByteArray projectsHistory = FileManager::openFile(&fileContext);

        return QString::fromUtf8(projectsHistory).split('\n');
    }

    void ProjectsHistoryManager::saveProjectsHistory(const QString &projectsHistory) {
        QStringList projects = loadProjectsHistory();

        projects.removeAll(projectsHistory);
        projects.prepend(projectsHistory);

        if (projects.size() > maxLength) projects = projects.mid(0, maxLength);

        QByteArray data;

        for (const QString & project : projects) data += project.toUtf8() + '\n';

        FileContext fileContext(getDefaultPathLocation());
        FileManager::saveFile(&fileContext, &data);
    }
} // utils