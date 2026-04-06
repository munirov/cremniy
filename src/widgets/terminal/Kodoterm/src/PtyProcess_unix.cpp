// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "PtyProcess_unix.h"

#include <QCoreApplication>
#include <QDebug>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <util.h>
#else
#include <pty.h>
#endif
#include <QFile>
#include <QFileInfo>
#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

PtyProcessUnix::PtyProcessUnix(QObject *parent) : PtyProcess(parent) {}

PtyProcessUnix::~PtyProcessUnix() {
    kill();
    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
}

bool PtyProcessUnix::start(const QSize &size) {
    if (m_program.isEmpty()) {
        return false;
    }

    struct winsize ws;
    ws.ws_row = (unsigned short)size.height();
    ws.ws_col = (unsigned short)size.width();
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    pid_t pid = forkpty(&m_masterFd, nullptr, nullptr, &ws);

    if (pid == -1) {
        qWarning() << "Failed to forkpty";
        return false;
    }

    if (pid == 0) {
        // Child
        // Working directory
        if (!m_workingDirectory.isEmpty()) {
            if (chdir(m_workingDirectory.toLocal8Bit().constData()) != 0) {
                _exit(1);
            }
        }

        // Environment
        for (const auto &key : m_environment.keys()) {
            setenv(key.toLocal8Bit().constData(),
                   m_environment.value(key).toLocal8Bit().constData(), 1);
        }
        setenv("TERM", "xterm-256color", 1);

        // Convert args to char* array
        std::vector<char *> args;
        QByteArray progBytes = m_program.toLocal8Bit();
        args.push_back(progBytes.data());

        // Helper to keep storage alive
        std::vector<QByteArray> storage;
        storage.reserve(m_arguments.size());

        for (const auto &arg : m_arguments) {
            storage.push_back(arg.toLocal8Bit());
            args.push_back(storage.back().data());
        }
        args.push_back(nullptr);

        execvp(m_program.toLocal8Bit().constData(), args.data());

        // If execvp returns, it failed
        _exit(1);
    } else {
        // Parent
        m_pid = pid;

        m_notifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &PtyProcessUnix::onReadyRead);

        return true;
    }
}

bool PtyProcessUnix::start(const QString &program, const QStringList &arguments,
                           const QSize &size) {
    return PtyProcess::start(program, arguments, size);
}

void PtyProcessUnix::write(const QByteArray &data) {
    if (m_masterFd >= 0) {
        ::write(m_masterFd, data.constData(), data.size());
    }
}

void PtyProcessUnix::resize(const QSize &size) {
    if (m_masterFd >= 0) {
        struct winsize ws;
        ws.ws_row = (unsigned short)size.height();
        ws.ws_col = (unsigned short)size.width();
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(m_masterFd, TIOCSWINSZ, &ws);
    }
}

void PtyProcessUnix::kill() {
    if (m_pid > 0) {
        ::kill(m_pid, SIGTERM);
        // Wait? usually waitpid via signal handler, but for now just cleanup
        m_pid = -1;
    }
}

bool PtyProcessUnix::isRoot() const {
    if (m_masterFd < 0) {
        return false;
    }

    pid_t pgrp = tcgetpgrp(m_masterFd);
    if (pgrp <= 0) {
        // Fallback to initial pid
        if (m_pid <= 0) {
            return false;
        }
        pgrp = m_pid;
    }

    struct stat st;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d", (int)pgrp);
    if (stat(path, &st) == 0) {
        return st.st_uid == 0;
    }
    return false;
}

QString PtyProcessUnix::foregroundProcessName() const {
    if (m_masterFd < 0) {
        return QString();
    }

    pid_t pgrp = tcgetpgrp(m_masterFd);
    if (pgrp <= 0) {
        return QFileInfo(m_program).baseName();
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", (int)pgrp);
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        return QString::fromUtf8(file.readAll().trimmed());
    }

    // Fallback to initial program
    return QFileInfo(m_program).baseName();
}

void PtyProcessUnix::onReadyRead() {
    char buffer[4096];
    ssize_t len = ::read(m_masterFd, buffer, sizeof(buffer));

    if (len > 0) {
        emit readyRead(QByteArray(buffer, (int)len));
    } else if (len < 0 && errno != EAGAIN) {
        m_notifier->setEnabled(false);
        emit finished(-1, -1); // Error
    } else if (len == 0) {
        m_notifier->setEnabled(false);
        emit finished(0, 0); // EOF
    }
}