// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "AppConfig.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

static QString systemCurrentShell() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

#if defined(Q_OS_WIN)
    // Windows shell (usually cmd.exe or powershell)
    if (env.contains("ComSpec")) {
        return env.value("ComSpec");
    }
    return QString();
#else
    // Unix / Linux / macOS shell (bash, zsh, fish, etc.)
    if (env.contains("SHELL")) {
        return env.value("SHELL");
    }
    return QString();
#endif
}

QList<AppConfig::ShellInfo> AppConfig::detectedShells() {
    QList<ShellInfo> shells;

    auto add = [&](const QString &name, const QString &path) {
        QFileInfo newFi(path);
        if (!newFi.exists()) {
            return;
        }

        QString newCanonical = newFi.canonicalFilePath();
        for (const auto &existing : shells) {
            if (QFileInfo(existing.path).canonicalFilePath() == newCanonical) {
                return;
            }
        }
        shells.append({name, path});
    };

#ifdef Q_OS_WIN
    QStringList paths = QString::fromUtf8(qgetenv("PATH")).split(';');
    auto check = [&](const QString &name, const QString &exe) {
        QString p = QStandardPaths::findExecutable(exe, paths);
        if (!p.isEmpty()) {
            add(name, p);
        }
    };

    check("Command Prompt", "cmd.exe");
    check("PowerShell", "powershell.exe");
    // Check for git bash explicitly in common locations if not in PATH
    QString gitBash = "C:\\Program Files\\Git\\bin\\bash.exe";
    if (QFile::exists(gitBash)) {
        add("Git Bash", gitBash);
    }

#else
    QFile file("/etc/shells");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith('#')) {
                add(line, line);
            }
        }
    }
    // Fallback if /etc/shells is empty or unreadable
    if (shells.isEmpty()) {
        add("/bin/bash", "/bin/bash");
        add("/bin/sh", "/bin/sh");
    }
#endif
    return shells;
}

QList<AppConfig::ShellInfo> AppConfig::loadShells() {
    QSettings s;
    QList<ShellInfo> shells;
    int size = s.beginReadArray("Shells");

    QSet<QString> seenCanonicalPaths;

    if (size > 0) {
        for (int i = 0; i < size; ++i) {
            s.setArrayIndex(i);
            ShellInfo info;
            info.name = s.value("name").toString();
            info.path = s.value("path").toString();

            QFileInfo fi(info.path);
            if (fi.exists()) {
                QString canonical = fi.canonicalFilePath();
                if (!seenCanonicalPaths.contains(canonical)) {
                    shells.append(info);
                    seenCanonicalPaths.insert(canonical);
                }
            } else {
                // Keep if it doesn't exist (custom path?) or skip?
                // Let's keep distinct paths.
                if (!seenCanonicalPaths.contains(info.path)) {
                    shells.append(info);
                    seenCanonicalPaths.insert(info.path);
                }
            }
        }
        s.endArray();
    } else {
        // First run or empty, fallback to detected
        shells = detectedShells();
        saveShells(shells); // Save them for next time
    }
    return shells;
}

void AppConfig::saveShells(const QList<ShellInfo> &shells) {
    QSettings s;
    s.beginWriteArray("Shells");
    for (int i = 0; i < shells.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue("name", shells[i].name);
        s.setValue("path", shells[i].path);
    }
    s.endArray();
}

QString AppConfig::defaultShell() {
    QSettings s;
    QString def = s.value("DefaultShell").toString();
    if (def.isEmpty()) {
        def = systemCurrentShell();
        /*        QList<ShellInfo> shells = loadShells();
                if (!shells.isEmpty()) {
                    def = shells.first().name;
                }*/
    }
    return def;
}

void AppConfig::setDefaultShell(const QString &name) {
    QSettings s;
    s.setValue("DefaultShell", name);
}

void AppConfig::cleanupOldLogs(int daysToKeep) {
    QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/KodoShell";
    QDir dir(logDir);
    if (!dir.exists()) {
        return;
    }

    QDateTime limit = QDateTime::currentDateTime().addDays(-daysToKeep);
    QStringList filters;
    filters << "kodoterm_*.log";

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    for (const auto &fi : files) {
        if (fi.lastModified() < limit) {
            QFile::remove(fi.absoluteFilePath());
        }
    }
}

AppConfig::ShellInfo AppConfig::getShellInfo(const QString &shellName) {
    QList<ShellInfo> shells = loadShells();
    for (const auto &info : shells) {
        if (info.name == shellName) {
            return info;
        }
    }
    return {shellName, shellName}; // Fallback
}