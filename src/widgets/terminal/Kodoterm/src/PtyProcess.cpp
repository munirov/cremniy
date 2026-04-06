// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "PtyProcess.h"
#include <QtGlobal>

#if defined(Q_OS_UNIX)
#include "PtyProcess_unix.h"
#elif defined(Q_OS_WIN)
#include "PtyProcess_win.h"
#endif

bool PtyProcess::start(const QString &program, const QStringList &arguments, const QSize &size) {
    setProgram(program);
    setArguments(arguments);
    return start(size);
}

PtyProcess *PtyProcess::create(QObject *parent) {
#if defined(Q_OS_UNIX)
    return new PtyProcessUnix(parent);
#elif defined(Q_OS_WIN)
    return new PtyProcessWin(parent);
#else
    return nullptr;
#endif
}
