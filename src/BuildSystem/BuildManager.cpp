#include "BuildManager.h"
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>

BuildManager::BuildManager(QObject* parent) : QObject(parent) {
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString out = QString::fromUtf8(m_process.readAllStandardOutput());
        for (const QString& line : out.split('\n', Qt::SkipEmptyParts))
            emit outputLine(line);
    });

    connect(&m_process, &QProcess::readyReadStandardError, this, [this]() {
        const QString err = QString::fromUtf8(m_process.readAllStandardError());
        for (const QString& line : err.split('\n', Qt::SkipEmptyParts))
            emit outputLine("[stderr] " + line);
    });

    connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                emit processFinished(code);
            });

    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        emit errorOccurred(m_process.errorString());
    });
}

void BuildManager::setProjectDir(const QString& dir) { m_projectDir = dir; }
void BuildManager::setConfig(const BuildConfig& cfg) { m_config = cfg; }

bool BuildManager::isRunning() const {
    return m_process.state() != QProcess::NotRunning;
}

void BuildManager::runBuild() { startCommand(m_config.build); }
void BuildManager::runRun()   { startCommand(m_config.run);   }
void BuildManager::runClean() { startCommand(m_config.clean); }

void BuildManager::stopProcess() {
    if (isRunning()) {
        m_process.terminate();
        if (!m_process.waitForFinished(2000))
            m_process.kill();
    }
}

void BuildManager::startCommand(const QString& cmd) {
    if (cmd.trimmed().isEmpty()) {
        emit errorOccurred("Command is not configured.");
        return;
    }
    if (isRunning()) {
        emit errorOccurred("A process is already running.");
        return;
    }

    m_process.setWorkingDirectory(m_projectDir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    const QString qtPath = m_config.qtPath.trimmed();
    if (!qtPath.isEmpty()) {
        const QString normalizedQtPath = QDir::fromNativeSeparators(qtPath);
        env.insert("CMAKE_PREFIX_PATH", normalizedQtPath);

        const QString qt6Dir = QDir(normalizedQtPath).filePath("lib/cmake/Qt6");
        const QString qt5Dir = QDir(normalizedQtPath).filePath("lib/cmake/Qt5");
        if (QFileInfo::exists(QDir(qt6Dir).filePath("Qt6Config.cmake"))) {
            env.insert("QT_DIR", QDir::fromNativeSeparators(qt6Dir));
            env.insert("Qt6_DIR", QDir::fromNativeSeparators(qt6Dir));
        } else if (QFileInfo::exists(QDir(qt5Dir).filePath("Qt5Config.cmake"))) {
            env.insert("QT_DIR", QDir::fromNativeSeparators(qt5Dir));
            env.insert("Qt5_DIR", QDir::fromNativeSeparators(qt5Dir));
        }
    }

    m_process.setProcessEnvironment(env);
    emit processStarted(cmd);
    emit outputLine("$ " + cmd);

#ifdef Q_OS_WIN
    m_process.start("cmd.exe", QStringList{"/C", cmd});
#else
    m_process.start("sh", QStringList{"-c", cmd});
#endif
}
