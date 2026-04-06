// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include <QString>
#include <QStringList>

class AppConfig {
  public:
    struct ShellInfo {
        QString name;
        QString path;
    };

    static QList<ShellInfo> detectedShells();
    static QList<ShellInfo> loadShells();
    static void saveShells(const QList<ShellInfo> &shells);

    static QString defaultShell();
    static void setDefaultShell(const QString &name);

    static ShellInfo getShellInfo(const QString &shellName);

    static void cleanupOldLogs(int daysToKeep = 7);

    // Session Management
};
