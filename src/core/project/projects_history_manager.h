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

#ifndef CREMNIY_PROJECTS_HISTORY_MANAGER_H
#define CREMNIY_PROJECTS_HISTORY_MANAGER_H

#include <QDebug>

namespace utils {

    struct RecentProject {
        QString path;
        QString name;
        QString language;
        QString lastOpenedAt;
    };

    class ProjectsHistoryManager {
    private:
        static constexpr unsigned short MAX_LENGTH = 15;

        ProjectsHistoryManager() = default;

        static QString getDefaultPathLocation();
        static QList<RecentProject> loadRawProjectsHistory();
        static void saveRawProjectsHistory(const QList<RecentProject>& projects);

    public:
        static QList<RecentProject> loadProjectsHistory();
        static void saveProjectHistory(const RecentProject& project);
        static void removeProjectFromHistory(const QString& projectPath);
        static void checkDirectoryExists();
    };

} /* namespace utils */

#endif /* CREMNIY_PROJECTS_HISTORY_MANAGER_H */