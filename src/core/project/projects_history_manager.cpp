/*
 * This file is part of the Cremniy IDE source code.
 *
 * Copyright (c) 2026 Cremniy IDE
 * SPDX-License-Identifier: GPL-3.0 license
 *
 * Repository:
 * https://github.com/munirov/cremniy
 *
 * Created by Dmitriy on 2026-03-27
 * Modified by Ilya (https://github.com/kykyrudza) on 2026-05-12
 */

#include "projects_history_manager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QStandardPaths>

namespace utils {

    QString ProjectsHistoryManager::getDefaultPathLocation() {
            return QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation
            ) + "/recent_projects.json";
    }

    QList<RecentProject> ProjectsHistoryManager::loadRawProjectsHistory() {
        QList<RecentProject> projects;

        QFile file(getDefaultPathLocation());

        if (!file.exists()) {
            return projects;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            return projects;
        }

        const auto data = file.readAll();
        file.close();

        const auto document = QJsonDocument::fromJson(data);

        if (!document.isObject()) {
            return projects;
        }

        const auto rootObject = document.object();

        if (!rootObject.contains("recentProjects")) {
            return projects;
        }

        const auto recentProjects = rootObject["recentProjects"].toArray();

        for (const auto& value : recentProjects) {
            const auto projectObject = value.toObject();

            RecentProject project;

            project.path = projectObject["path"].toString();
            project.name = projectObject["name"].toString();
            project.language = projectObject["language"].toString();
            project.lastOpenedAt = projectObject["lastOpenedAt"].toString();

            projects.append(project);
        }

        return projects;
    }

    void ProjectsHistoryManager::saveRawProjectsHistory(
        const QList<RecentProject>& projects
    )
    {
        QJsonArray projectsArray;

        for (const auto& [path, name, language, lastOpenedAt] : projects) {
            QJsonObject projectObject;

            projectObject["path"] = path;
            projectObject["name"] = name;
            projectObject["language"] = language;
            projectObject["lastOpenedAt"] = lastOpenedAt;

            projectsArray.append(projectObject);
        }

        QJsonObject rootObject;
        rootObject["recentProjects"] = projectsArray;

        const QJsonDocument document(rootObject);

        QDir().mkpath(
            QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation
            )
        );

        QFile file(getDefaultPathLocation());

        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return;
        }

        file.write(document.toJson(QJsonDocument::Indented));
        file.close();
    }

    QList<RecentProject> ProjectsHistoryManager::loadProjectsHistory() {
        checkDirectoryExists();

        return loadRawProjectsHistory();
    }

    void ProjectsHistoryManager::saveProjectHistory(
        const RecentProject& project
    )
    {
        auto projects = loadProjectsHistory();

        for (auto i = 0; i < projects.size(); ++i) {
            if (projects[i].path == project.path) {
                projects.removeAt(i);
                break;
            }
        }

        auto updatedProject = project;

        updatedProject.lastOpenedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

        projects.prepend(updatedProject);

        if (projects.size() > MAX_LENGTH) {
            projects = projects.mid(0, MAX_LENGTH);
        }

        saveRawProjectsHistory(projects);
    }

    void ProjectsHistoryManager::checkDirectoryExists()
    {
        const auto projects = loadRawProjectsHistory();

        QList<RecentProject> validProjects;

        for (const auto& project : projects) {
            if (QDir(project.path).exists()) {
                validProjects.append(project);
            }
        }

        saveRawProjectsHistory(validProjects);
    }

    void ProjectsHistoryManager::removeProjectFromHistory(
        const QString& projectPath
    )
    {
        auto projects = loadProjectsHistory();

        for (auto i = 0; i < projects.size(); ++i) {
            if (projects[i].path == projectPath) {
                projects.removeAt(i);
                break;
            }
        }

        saveRawProjectsHistory(projects);
    }

} /* namespace utils */