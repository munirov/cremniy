#pragma once
#include "BuildConfig.h"
#include <QObject>
#include <QProcess>

// Сигналы выводятся в OutputPanel (QPlainTextEdit снизу IDE).
class BuildManager : public QObject {
    Q_OBJECT
public:
    explicit BuildManager(QObject* parent = nullptr);

    void setProjectDir(const QString& dir);
    void setConfig(const BuildConfig& cfg);
    const BuildConfig& config() const { return m_config; }

    bool isRunning() const;

public slots:
    void runBuild();
    void runRun();
    void runClean();
    void stopProcess();

signals:
    void outputLine(const QString& line);    // строка stdout/stderr
    void processStarted(const QString& cmd);
    void processFinished(int exitCode);
    void errorOccurred(const QString& msg);

private:
    void startCommand(const QString& cmd);

    QProcess  m_process;
    BuildConfig m_config;
    QString   m_projectDir;
};