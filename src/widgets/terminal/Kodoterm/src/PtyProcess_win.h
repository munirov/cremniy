// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "PtyProcess.h"
#include <QThread>
#include <windows.h>

class PtyProcessWin : public PtyProcess {
    Q_OBJECT

  public:
    explicit PtyProcessWin(QObject *parent = nullptr);
    ~PtyProcessWin() override;

    bool start(const QSize &size) override;
    bool start(const QString &program, const QStringList &arguments, const QSize &size) override;
    void write(const QByteArray &data) override;
    void resize(const QSize &size) override;
    void kill() override;
    bool isRoot() const override;
    QString foregroundProcessName() const override;

  private slots:
    void onReadThreadData(const QByteArray &data);

  private:
    HPCON m_hPC = INVALID_HANDLE_VALUE;
    HANDLE m_hPipeIn = INVALID_HANDLE_VALUE;
    HANDLE m_hPipeOut = INVALID_HANDLE_VALUE;
    PROCESS_INFORMATION m_pi;

    class ReaderThread;
    ReaderThread *m_readerThread = nullptr;
};
