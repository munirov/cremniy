#include "ptyprocess_windows.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QWinEventNotifier>

#include <algorithm>
#include <atomic>
#include <vector>

#include <qt_windows.h>

namespace Cremniy::Terminal {

using PseudoConsoleHandle = void *;
using CreatePseudoConsoleFunction = HRESULT(WINAPI *)(COORD, HANDLE, HANDLE, DWORD,
                                                       PseudoConsoleHandle *);
using ResizePseudoConsoleFunction = HRESULT(WINAPI *)(PseudoConsoleHandle, COORD);
using ClosePseudoConsoleFunction = void(WINAPI *)(PseudoConsoleHandle);

struct PtyProcessWindows::NativeState
{
    HMODULE kernel32 = nullptr;
    CreatePseudoConsoleFunction createPseudoConsole = nullptr;
    ResizePseudoConsoleFunction resizePseudoConsole = nullptr;
    ClosePseudoConsoleFunction closePseudoConsole = nullptr;

    PseudoConsoleHandle pseudoConsole = nullptr;
    HANDLE inputWrite = INVALID_HANDLE_VALUE;
    HANDLE outputRead = INVALID_HANDLE_VALUE;
    HANDLE process = INVALID_HANDLE_VALUE;
    DWORD processId = 0;
    LPPROC_THREAD_ATTRIBUTE_LIST attributeList = nullptr;
    QWinEventNotifier *processNotifier = nullptr;
    QThread *readerThread = nullptr;
    std::atomic<DWORD> readerThreadId{0};
    QMutex writeMutex;
};

static void closeNativeHandle(HANDLE &handle)
{
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
        ::CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
}

static QString windowsErrorMessage(const QString &operation, DWORD code = ::GetLastError())
{
    wchar_t *message = nullptr;
    const DWORD length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                                              | FORMAT_MESSAGE_FROM_SYSTEM
                                              | FORMAT_MESSAGE_IGNORE_INSERTS,
                                          nullptr,
                                          code,
                                          0,
                                          reinterpret_cast<wchar_t *>(&message),
                                          0,
                                          nullptr);
    QString detail = length ? QString::fromWCharArray(message, static_cast<qsizetype>(length)).trimmed()
                            : QString::number(code);
    if (message)
        ::LocalFree(message);
    return QStringLiteral("%1: %2").arg(operation, detail);
}

static QString quoteWindowsArgument(const QString &argument)
{
    if (!argument.isEmpty()
        && !argument.contains(QRegularExpression(QStringLiteral("[\\s\"]")))) {
        return argument;
    }

    QString quoted = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar ch : argument) {
        if (ch == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            quoted += QString(backslashes * 2 + 1, QLatin1Char('\\'));
            quoted += ch;
            backslashes = 0;
            continue;
        }
        quoted += QString(backslashes, QLatin1Char('\\'));
        backslashes = 0;
        quoted += ch;
    }
    quoted += QString(backslashes * 2, QLatin1Char('\\'));
    quoted += QLatin1Char('"');
    return quoted;
}

static std::vector<wchar_t> environmentBlock(const QProcessEnvironment &environment)
{
    QStringList entries = environment.toStringList();
    std::sort(entries.begin(), entries.end(), [](const QString &left, const QString &right) {
        return QString::compare(left, right, Qt::CaseInsensitive) < 0;
    });
    std::vector<wchar_t> result;
    for (const QString &entry : entries) {
        const qsizetype oldSize = static_cast<qsizetype>(result.size());
        result.resize(static_cast<size_t>(oldSize + entry.size() + 1));
        entry.toWCharArray(result.data() + oldSize);
        result[static_cast<size_t>(oldSize + entry.size())] = L'\0';
    }
    result.push_back(L'\0');
    return result;
}

PtyProcessWindows::PtyProcessWindows(QObject *parent)
    : PtyProcess(parent)
    , m_native(std::make_unique<NativeState>())
{
}

PtyProcessWindows::~PtyProcessWindows()
{
    stop();
}

bool PtyProcessWindows::start(const PtyProcessOptions &options)
{
    if (m_running)
        return fail(tr("A terminal process is already running."));
    if (!QFileInfo(options.executable).isExecutable())
        return fail(tr("Shell is not executable: %1").arg(options.executable));
    if (!options.workingDirectory.isEmpty() && !QDir(options.workingDirectory).exists())
        return fail(tr("Working directory does not exist: %1").arg(options.workingDirectory));

    m_native->kernel32 = ::GetModuleHandleW(L"kernel32.dll");
    m_native->createPseudoConsole = reinterpret_cast<CreatePseudoConsoleFunction>(
        ::GetProcAddress(m_native->kernel32, "CreatePseudoConsole"));
    m_native->resizePseudoConsole = reinterpret_cast<ResizePseudoConsoleFunction>(
        ::GetProcAddress(m_native->kernel32, "ResizePseudoConsole"));
    m_native->closePseudoConsole = reinterpret_cast<ClosePseudoConsoleFunction>(
        ::GetProcAddress(m_native->kernel32, "ClosePseudoConsole"));
    if (!m_native->createPseudoConsole || !m_native->resizePseudoConsole
        || !m_native->closePseudoConsole) {
        return fail(tr("ConPTY requires Windows 10 version 1809 or newer."));
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE inputRead = INVALID_HANDLE_VALUE;
    HANDLE outputWrite = INVALID_HANDLE_VALUE;

    if (!::CreatePipe(&inputRead, &m_native->inputWrite, &attributes, 0)
        || !::CreatePipe(&m_native->outputRead, &outputWrite, &attributes, 0)) {
        closeNativeHandle(inputRead);
        closeNativeHandle(outputWrite);
        releaseHandles();
        return fail(windowsErrorMessage(tr("Unable to create ConPTY pipes")));
    }
    ::SetHandleInformation(m_native->inputWrite, HANDLE_FLAG_INHERIT, 0);
    ::SetHandleInformation(m_native->outputRead, HANDLE_FLAG_INHERIT, 0);

    const QSize size(qBound(1, options.initialSize.width(), 32767),
                     qBound(1, options.initialSize.height(), 32767));
    const HRESULT createResult = m_native->createPseudoConsole(
        COORD{static_cast<SHORT>(size.width()), static_cast<SHORT>(size.height())},
        inputRead,
        outputWrite,
        0,
        &m_native->pseudoConsole);
    if (FAILED(createResult)) {
        closeNativeHandle(inputRead);
        closeNativeHandle(outputWrite);
        releaseHandles();
        return fail(windowsErrorMessage(tr("Unable to create ConPTY"),
                                        HRESULT_CODE(createResult)));
    }

    SIZE_T attributeBytes = 0;
    ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
    m_native->attributeList = static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        ::HeapAlloc(::GetProcessHeap(), 0, attributeBytes));
    if (!m_native->attributeList
        || !::InitializeProcThreadAttributeList(m_native->attributeList,
                                                1,
                                                0,
                                                &attributeBytes)
        || !::UpdateProcThreadAttribute(m_native->attributeList,
                                        0,
                                        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                        m_native->pseudoConsole,
                                        sizeof(m_native->pseudoConsole),
                                        nullptr,
                                        nullptr)) {
        closeNativeHandle(inputRead);
        closeNativeHandle(outputWrite);
        releaseHandles();
        return fail(windowsErrorMessage(tr("Unable to configure ConPTY process")));
    }

    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.hStdInput = m_native->outputRead;
    startup.StartupInfo.hStdOutput = m_native->inputWrite;
    startup.StartupInfo.hStdError = m_native->inputWrite;
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.lpAttributeList = m_native->attributeList;

    QStringList commandParts{QDir::toNativeSeparators(options.executable)};
    commandParts.append(options.arguments);
    QString commandLine;
    for (const QString &part : commandParts) {
        if (!commandLine.isEmpty())
            commandLine += QLatin1Char(' ');
        commandLine += quoteWindowsArgument(part);
    }
    std::vector<wchar_t> mutableCommand(static_cast<size_t>(commandLine.size() + 1));
    commandLine.toWCharArray(mutableCommand.data());
    mutableCommand.back() = L'\0';
    std::vector<wchar_t> environment = environmentBlock(options.environment);
    const std::wstring executable = QDir::toNativeSeparators(options.executable).toStdWString();
    const std::wstring workingDirectory = QDir::toNativeSeparators(options.workingDirectory).toStdWString();

    PROCESS_INFORMATION processInfo{};
    const BOOL created = ::CreateProcessW(executable.c_str(),
                                          mutableCommand.data(),
                                          nullptr,
                                          nullptr,
                                          FALSE,
                                          EXTENDED_STARTUPINFO_PRESENT
                                              | CREATE_UNICODE_ENVIRONMENT,
                                          environment.data(),
                                          workingDirectory.empty() ? nullptr
                                                                   : workingDirectory.c_str(),
                                          &startup.StartupInfo,
                                          &processInfo);
    closeNativeHandle(inputRead);
    closeNativeHandle(outputWrite);
    if (!created) {
        releaseHandles();
        return fail(windowsErrorMessage(tr("Unable to start shell")));
    }
    ::CloseHandle(processInfo.hThread);
    m_native->process = processInfo.hProcess;
    m_native->processId = processInfo.dwProcessId;
    m_running = true;
    m_finishedEmitted = false;

    m_native->processNotifier = new QWinEventNotifier(m_native->process, this);
    connect(m_native->processNotifier, &QWinEventNotifier::activated, this, [this] {
        if (!m_running)
            return;
        DWORD exitCode = 0;
        ::GetExitCodeProcess(m_native->process, &exitCode);
        m_running = false;
        m_native->processNotifier->setEnabled(false);
        closePseudoConsole();
        if (!m_finishedEmitted) {
            m_finishedEmitted = true;
            emit finished(static_cast<int>(exitCode));
        }
    });

    m_native->readerThread = QThread::create([this] {
        m_native->readerThreadId.store(::GetCurrentThreadId());
        char buffer[16 * 1024];
        for (;;) {
            DWORD bytesRead = 0;
            const BOOL ok = ::ReadFile(m_native->outputRead,
                                       buffer,
                                       static_cast<DWORD>(sizeof(buffer)),
                                       &bytesRead,
                                       nullptr);
            if (ok && bytesRead > 0) {
                const QByteArray data(buffer, static_cast<qsizetype>(bytesRead));
                QMetaObject::invokeMethod(this,
                                          [this, data] { emit dataReceived(data); },
                                          Qt::QueuedConnection);
                continue;
            }
            if (!ok && ::GetLastError() == ERROR_MORE_DATA)
                continue;
            break;
        }
        m_native->readerThreadId.store(0);
    });
    m_native->readerThread->start();

    emit started(static_cast<qint64>(m_native->processId));
    return true;
}

qint64 PtyProcessWindows::write(const QByteArray &data)
{
    if (!m_running || m_native->inputWrite == INVALID_HANDLE_VALUE || data.isEmpty())
        return 0;
    QMutexLocker locker(&m_native->writeMutex);
    DWORD bytesWritten = 0;
    if (!::WriteFile(m_native->inputWrite,
                     data.constData(),
                     static_cast<DWORD>(data.size()),
                     &bytesWritten,
                     nullptr)) {
        fail(windowsErrorMessage(tr("Unable to write to ConPTY")));
        return 0;
    }
    return static_cast<qint64>(bytesWritten);
}

bool PtyProcessWindows::resize(const QSize &requestedSize)
{
    if (!m_native->pseudoConsole)
        return false;
    const QSize size(qBound(1, requestedSize.width(), 32767),
                     qBound(1, requestedSize.height(), 32767));
    const HRESULT result = m_native->resizePseudoConsole(
        m_native->pseudoConsole,
        COORD{static_cast<SHORT>(size.width()), static_cast<SHORT>(size.height())});
    if (SUCCEEDED(result))
        return true;
    fail(windowsErrorMessage(tr("Unable to resize ConPTY"), HRESULT_CODE(result)));
    return false;
}

void PtyProcessWindows::stop()
{
    if (m_running && m_native->inputWrite != INVALID_HANDLE_VALUE) {
        write(QByteArray(1, '\x03'));
        if (::WaitForSingleObject(m_native->process, 250) == WAIT_TIMEOUT)
            ::TerminateProcess(m_native->process, 1);
    }
    m_running = false;
    closePseudoConsole();
    stopReader();
    releaseHandles();
}

bool PtyProcessWindows::isRunning() const
{
    return m_running;
}

qint64 PtyProcessWindows::processId() const
{
    return static_cast<qint64>(m_native->processId);
}

QString PtyProcessWindows::lastError() const
{
    return m_lastError;
}

bool PtyProcessWindows::fail(const QString &message)
{
    m_lastError = message;
    emit errorOccurred(message);
    return false;
}

void PtyProcessWindows::closePseudoConsole()
{
    if (m_native->pseudoConsole && m_native->closePseudoConsole) {
        m_native->closePseudoConsole(m_native->pseudoConsole);
        m_native->pseudoConsole = nullptr;
    }
}

void PtyProcessWindows::stopReader()
{
    if (!m_native->readerThread)
        return;
    const DWORD threadId = m_native->readerThreadId.load();
    if (threadId != 0) {
        HANDLE thread = ::OpenThread(THREAD_TERMINATE, FALSE, threadId);
        if (thread) {
            ::CancelSynchronousIo(thread);
            ::CloseHandle(thread);
        }
    }
    closeNativeHandle(m_native->outputRead);
    m_native->readerThread->wait(2000);
    delete m_native->readerThread;
    m_native->readerThread = nullptr;
}

void PtyProcessWindows::releaseHandles()
{
    delete m_native->processNotifier;
    m_native->processNotifier = nullptr;
    closeNativeHandle(m_native->inputWrite);
    closeNativeHandle(m_native->outputRead);
    closeNativeHandle(m_native->process);
    m_native->processId = 0;
    if (m_native->attributeList) {
        ::DeleteProcThreadAttributeList(m_native->attributeList);
        ::HeapFree(::GetProcessHeap(), 0, m_native->attributeList);
        m_native->attributeList = nullptr;
    }
    closePseudoConsole();
}

} // namespace Cremniy::Terminal
