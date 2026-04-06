// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#pragma once

#include "PtyProcess.h"
#include <QSocketNotifier>
#include <sys/types.h>

class PtyProcessUnix : public PtyProcess {
    Q_OBJECT

  public:
    PtyProcessUnix(QObject *parent = nullptr);
    ~PtyProcessUnix();

    bool start(const QSize &size) override;
    bool start(const QString &program, const QStringList &arguments, const QSize &size) override;
    void write(const QByteArray &data) override;
    void resize(const QSize &size) override;
    void kill() override;
    bool isRoot() const override;
    QString foregroundProcessName() const override;

  private slots:
    void onReadyRead();

  private:
    int m_masterFd = -1;
    pid_t m_pid = -1;
    QSocketNotifier *m_notifier = nullptr;
};
