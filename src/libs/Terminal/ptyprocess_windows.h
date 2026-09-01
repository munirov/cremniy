#pragma once

#include "ptyprocess.h"

#include <memory>

namespace Cremniy::Terminal {

class PtyProcessWindows final : public PtyProcess
{
    Q_OBJECT

public:
    explicit PtyProcessWindows(QObject *parent = nullptr);
    ~PtyProcessWindows() override;

    bool start(const PtyProcessOptions &options) override;
    qint64 write(const QByteArray &data) override;
    bool resize(const QSize &size) override;
    void stop() override;

    bool isRunning() const override;
    qint64 processId() const override;
    QString lastError() const override;

private:
    struct NativeState;
    bool fail(const QString &message);
    void closePseudoConsole();
    void stopReader();
    void releaseHandles();

    std::unique_ptr<NativeState> m_native;
    QString m_lastError;
    bool m_running = false;
    bool m_finishedEmitted = false;
};

} // namespace Cremniy::Terminal
