#include "buildmanager.h"


BuildManager::BuildManager(const QString &workPath, const QString &command, logView* logViewWidg)
    : m_workPath(workPath),
      m_command(command)
{
    m_buildProcess = new QProcess(this);
    m_buildProcess->setWorkingDirectory(m_workPath);

    // данные в stdout
    connect(m_buildProcess, &QProcess::readyReadStandardOutput, this, [this, logViewWidg]() {
        QString output = QString::fromLocal8Bit(m_buildProcess->readAllStandardOutput());
        logViewWidg->onOutputReceived(output);
    });

    // данные в stderr
    connect(m_buildProcess, &QProcess::readyReadStandardError, this, [this, logViewWidg]() {
        QString error = QString::fromLocal8Bit(m_buildProcess->readAllStandardError());
        logViewWidg->onErrorReceived(error);
    });

    // завершение процесса сборки
    connect(m_buildProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this, logViewWidg]() {
        logViewWidg->onOutputReceived("Finish");
    });
}

BuildManager::~BuildManager() {
    stop();
}


void BuildManager::build(){
    if (m_buildProcess->state() != QProcess::NotRunning) {
        return;
    }

    #if defined(Q_OS_WIN)
        m_buildProcess->start("cmd.exe", QStringList() << "/c" << m_command);
    #else
        m_buildProcess->start("sh", QStringList() << "-c" << m_command);
    #endif

}

void BuildManager::stop(){
    if (m_buildProcess->state() == QProcess::Running) {
        m_buildProcess->kill();
    }
}
