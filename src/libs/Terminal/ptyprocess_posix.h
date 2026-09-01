#pragma once

#include "ptyprocess.h"

#include <QProcess>

class QSocketNotifier;

namespace Cremniy::Terminal {

class PtyProcessPosix final : public PtyProcess
{
    Q_OBJECT

public:
    explicit PtyProcessPosix(QObject *parent = nullptr);
    ~PtyProcessPosix() override;

    bool start(const PtyProcessOptions &options) override;
    qint64 write(const QByteArray &data) override;
    bool resize(const QSize &size) override;
    void stop() override;

    bool isRunning() const override;
    qint64 processId() const override;
    QString lastError() const override;

private:
    void readAvailable();
    void closeTerminalDescriptors();
    bool fail(const QString &message);

    QProcess m_process;
    QSocketNotifier *m_readNotifier = nullptr;
    int m_masterFd = -1;
    int m_slaveFd = -1;
    QString m_lastError;
};

} // namespace Cremniy::Terminal
