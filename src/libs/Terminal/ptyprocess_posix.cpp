#include "ptyprocess_posix.h"

#include <QDir>
#include <QFileInfo>
#include <QSocketNotifier>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef Q_OS_MACOS
#include <util.h>
#else
#include <pty.h>
#endif

namespace Cremniy::Terminal {

static QString systemError(const QString &operation)
{
    return QStringLiteral("%1: %2").arg(operation, QString::fromLocal8Bit(std::strerror(errno)));
}

PtyProcessPosix::PtyProcessPosix(QObject *parent)
    : PtyProcess(parent)
{
    connect(&m_process, &QProcess::finished, this, [this](int exitCode) {
        readAvailable();
        closeTerminalDescriptors();
        emit finished(exitCode);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(m_process.errorString());
    });
}

PtyProcessPosix::~PtyProcessPosix()
{
    stop();
}

bool PtyProcessPosix::start(const PtyProcessOptions &options)
{
    if (isRunning())
        return fail(tr("A terminal process is already running."));
    if (!QFileInfo(options.executable).isExecutable())
        return fail(tr("Shell is not executable: %1").arg(options.executable));
    if (!options.workingDirectory.isEmpty() && !QDir(options.workingDirectory).exists())
        return fail(tr("Working directory does not exist: %1").arg(options.workingDirectory));

    const QSize size(qBound(1, options.initialSize.width(), 32767),
                     qBound(1, options.initialSize.height(), 32767));
    winsize terminalSize{};
    terminalSize.ws_col = static_cast<unsigned short>(size.width());
    terminalSize.ws_row = static_cast<unsigned short>(size.height());

    if (::openpty(&m_masterFd, &m_slaveFd, nullptr, nullptr, &terminalSize) != 0)
        return fail(systemError(tr("Unable to create POSIX PTY")));

    const int currentFlags = ::fcntl(m_masterFd, F_GETFL, 0);
    if (currentFlags < 0 || ::fcntl(m_masterFd, F_SETFL, currentFlags | O_NONBLOCK) < 0) {
        const QString message = systemError(tr("Unable to configure POSIX PTY"));
        closeTerminalDescriptors();
        return fail(message);
    }
    ::fcntl(m_masterFd, F_SETFD, FD_CLOEXEC);
    ::fcntl(m_slaveFd, F_SETFD, FD_CLOEXEC);

    const int masterFd = m_masterFd;
    const int slaveFd = m_slaveFd;
    QProcess *process = &m_process;
    m_process.setChildProcessModifier([process, masterFd, slaveFd] {
        ::close(masterFd);
        if (::setsid() < 0)
            process->failChildProcessModifier("setsid", errno);
        if (::ioctl(slaveFd, TIOCSCTTY, 0) < 0)
            process->failChildProcessModifier("TIOCSCTTY", errno);
        if (::dup2(slaveFd, STDIN_FILENO) < 0
            || ::dup2(slaveFd, STDOUT_FILENO) < 0
            || ::dup2(slaveFd, STDERR_FILENO) < 0) {
            process->failChildProcessModifier("dup2", errno);
        }
        if (slaveFd > STDERR_FILENO)
            ::close(slaveFd);
    });

    m_process.setProgram(options.executable);
    m_process.setArguments(options.arguments);
    m_process.setWorkingDirectory(options.workingDirectory);
    m_process.setProcessEnvironment(options.environment);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);

    m_readNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &PtyProcessPosix::readAvailable);

    m_process.start();
    if (!m_process.waitForStarted(5000)) {
        const QString message = m_process.errorString();
        closeTerminalDescriptors();
        return fail(message);
    }

    ::close(m_slaveFd);
    m_slaveFd = -1;
    emit started(m_process.processId());
    return true;
}

qint64 PtyProcessPosix::write(const QByteArray &data)
{
    if (m_masterFd < 0 || data.isEmpty())
        return 0;
    const ssize_t written = ::write(m_masterFd, data.constData(), static_cast<size_t>(data.size()));
    if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        fail(systemError(tr("Unable to write to POSIX PTY")));
    return written < 0 ? 0 : written;
}

bool PtyProcessPosix::resize(const QSize &requestedSize)
{
    if (m_masterFd < 0)
        return false;
    const QSize size(qBound(1, requestedSize.width(), 32767),
                     qBound(1, requestedSize.height(), 32767));
    winsize terminalSize{};
    terminalSize.ws_col = static_cast<unsigned short>(size.width());
    terminalSize.ws_row = static_cast<unsigned short>(size.height());
    if (::ioctl(m_masterFd, TIOCSWINSZ, &terminalSize) == 0)
        return true;
    fail(systemError(tr("Unable to resize POSIX PTY")));
    return false;
}

void PtyProcessPosix::stop()
{
    if (m_process.state() != QProcess::NotRunning) {
        const qint64 pid = m_process.processId();
        if (pid > 0)
            ::kill(-static_cast<pid_t>(pid), SIGHUP);
        if (!m_process.waitForFinished(500) && pid > 0)
            ::kill(-static_cast<pid_t>(pid), SIGTERM);
        if (!m_process.waitForFinished(1000) && pid > 0)
            ::kill(-static_cast<pid_t>(pid), SIGKILL);
        m_process.waitForFinished(1000);
    }
    closeTerminalDescriptors();
}

bool PtyProcessPosix::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

qint64 PtyProcessPosix::processId() const
{
    return m_process.processId();
}

QString PtyProcessPosix::lastError() const
{
    return m_lastError;
}

void PtyProcessPosix::readAvailable()
{
    if (m_masterFd < 0)
        return;
    QByteArray result;
    char buffer[16 * 1024];
    for (;;) {
        const ssize_t count = ::read(m_masterFd, buffer, sizeof(buffer));
        if (count > 0) {
            result.append(buffer, static_cast<qsizetype>(count));
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        break;
    }
    if (!result.isEmpty())
        emit dataReceived(result);
}

void PtyProcessPosix::closeTerminalDescriptors()
{
    delete m_readNotifier;
    m_readNotifier = nullptr;
    if (m_slaveFd >= 0) {
        ::close(m_slaveFd);
        m_slaveFd = -1;
    }
    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
}

bool PtyProcessPosix::fail(const QString &message)
{
    m_lastError = message;
    emit errorOccurred(message);
    return false;
}

} // namespace Cremniy::Terminal
