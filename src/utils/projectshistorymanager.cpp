//
// Created by Dmitriy on 3/27/26.
//

#include "projectshistorymanager.h"

#include <QDir>

#include "filecontext.h"
#include "filemanager.h"

namespace utils {
    QStringList ProjectsHistoryManager::loadProjectsHistory() {
        checkDirectoryExists();
        return loadRawProjectsHistory();
    }

    QStringList ProjectsHistoryManager::loadRawProjectsHistory() {
        FileContext fileContext(getDefaultPathLocation());
        const QByteArray projectsHistory = FileManager::openFile(&fileContext);

        return QString::fromUtf8(projectsHistory).split('\n', Qt::SkipEmptyParts);
    }

    void ProjectsHistoryManager::saveProjectsHistory(const QString &projectsHistory) {
        QStringList projects = loadProjectsHistory();

        projects.removeAll(projectsHistory);
        projects.prepend(projectsHistory);

        if (projects.size() > maxLength) projects = projects.mid(0, maxLength);

        formatedDataRawAndSave(projects);
    }

    void ProjectsHistoryManager::formatedDataRawAndSave(const QStringList &formatedList) {
        QByteArray data;

        for (const QString & project : formatedList) data += project.toUtf8() + '\n';

        FileContext fileContext(getDefaultPathLocation());
        FileManager::saveFile(&fileContext, &data);
    }

    void ProjectsHistoryManager::checkDirectoryExists() {
        QStringList projects = loadRawProjectsHistory();
        QStringList formatedList;

        for (const QString & project : projects)
            if (QDir(project).exists())
                formatedList.append(project);

        formatedDataRawAndSave(formatedList);
    }
}; // utils