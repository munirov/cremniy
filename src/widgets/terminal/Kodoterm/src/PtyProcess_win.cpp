// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "PtyProcess_win.h"
#include <QDebug>
#include <QDir>
#include <vector>

// Define necessary types if building on older SDKs or mingw that might lack them
// But assuming standard modern environment.

class PtyProcessWin::ReaderThread : public QThread {
  public:
    ReaderThread(HANDLE hPipe, PtyProcessWin *parent) : m_hPipe(hPipe), m_parent(parent) {}

    void run() override {
        char buffer[4096];
        DWORD bytesRead;
        while (m_running) {
            if (ReadFile(m_hPipe, buffer, sizeof(buffer), &bytesRead, NULL)) {
                if (bytesRead > 0) {
                    QByteArray data(buffer, (int)bytesRead);
                    QMetaObject::invokeMethod(m_parent, "onReadThreadData", Qt::QueuedConnection,
                                              Q_ARG(QByteArray, data));
                } else {
                    // EOF (bytesRead == 0)
                    break;
                }
            } else {
                DWORD err = GetLastError();
                if (err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF) {
                    break;
                }
                break;
            }
        }
    }

    void stop() { m_running = false; }

  private:
    HANDLE m_hPipe;
    PtyProcessWin *m_parent;
    bool m_running = true;
};

PtyProcessWin::PtyProcessWin(QObject *parent) : PtyProcess(parent) {
    ZeroMemory(&m_pi, sizeof(PROCESS_INFORMATION));
    m_hPC = INVALID_HANDLE_VALUE;
    m_hPipeIn = INVALID_HANDLE_VALUE;
    m_hPipeOut = INVALID_HANDLE_VALUE;
}

PtyProcessWin::~PtyProcessWin() {
    kill(); // kill() now closes everything properly
}

bool PtyProcessWin::start(const QSize &size) {
    if (m_program.isEmpty()) {
        return false;
    }

    HANDLE hPipePTYIn = INVALID_HANDLE_VALUE;
    HANDLE hPipePTYOut = INVALID_HANDLE_VALUE;

    // Create pipes
    if (!CreatePipe(&hPipePTYIn, &m_hPipeOut, NULL, 0)) {
        return false;
    }
    if (!CreatePipe(&m_hPipeIn, &hPipePTYOut, NULL, 0)) {
        return false;
    }

    // Create Pseudo Console
    COORD origin = {(SHORT)size.width(), (SHORT)size.height()};
    HRESULT hr = CreatePseudoConsole(origin, hPipePTYIn, hPipePTYOut, 0, &m_hPC);

    // Close the sides we don't need
    CloseHandle(hPipePTYIn);
    CloseHandle(hPipePTYOut);

    if (FAILED(hr)) {
        return false;
    }

    // Prepare Startup Info
    STARTUPINFOEX si;
    ZeroMemory(&si, sizeof(STARTUPINFOEX));
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);

    SIZE_T bytesRequired = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList) {
        return false;
    }

    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)) {
        return false;
    }

    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   m_hPC, sizeof(HPCON), NULL, NULL)) {
        return false;
    }

    // Command Line
    QString programNative = QDir::toNativeSeparators(m_program);
    QString cmd = programNative;
    if (cmd.contains(' ')) {
        cmd = "\"" + cmd + "\"";
    }
    for (const auto &arg : m_arguments) {
        cmd += " " + arg; // Simple quoting might be needed
    }

    // Working directory
    wchar_t *pWorkingDirectory = nullptr;
    std::vector<wchar_t> workingDir;
    if (!m_workingDirectory.isEmpty()) {
        workingDir.resize(m_workingDirectory.length() + 1);
        m_workingDirectory.toWCharArray(workingDir.data());
        workingDir[m_workingDirectory.length()] = 0;
        pWorkingDirectory = workingDir.data();
    }

    // Environment
    std::vector<wchar_t> envBlock;
    for (const auto &key : m_environment.keys()) {
        QString entry = key + "=" + m_environment.value(key);
        for (QChar c : entry) {
            envBlock.push_back(c.unicode());
        }
        envBlock.push_back(0);
    }
    envBlock.push_back(0);

    // Create Process
    std::vector<wchar_t> cmdLine(cmd.length() + 1);
    cmd.toWCharArray(cmdLine.data());
    cmdLine[cmd.length()] = 0;

    BOOL success = CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE,
                                  EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
                                  envBlock.data(), pWorkingDirectory, &si.StartupInfo, &m_pi);

    // Cleanup attribute list
    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);

    if (!success) {
        return false;
    }

    // Start reader thread
    m_readerThread = new ReaderThread(m_hPipeIn, this);
    m_readerThread->start();

    return true;
}

bool PtyProcessWin::start(const QString &program, const QStringList &arguments, const QSize &size) {
    return PtyProcess::start(program, arguments, size);
}

void PtyProcessWin::write(const QByteArray &data) {
    if (m_hPipeOut != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        WriteFile(m_hPipeOut, data.constData(), data.size(), &bytesWritten, NULL);
    }
}

void PtyProcessWin::resize(const QSize &size) {
    if (m_hPC != INVALID_HANDLE_VALUE) {
        COORD origin = {(SHORT)size.width(), (SHORT)size.height()};
        ResizePseudoConsole(m_hPC, origin);
    }
}

void PtyProcessWin::kill() {
    // First, close the pseudo console — this is the key fix
    // It immediately unblocks any pending ReadFile on the pipes
    if (m_hPC != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(m_hPC);
        m_hPC = INVALID_HANDLE_VALUE;
    }

    // Stop and clean up reader thread
    if (m_readerThread) {
        m_readerThread->stop();
        // Close our read handle as a safety net (though ClosePseudoConsole already unblocked)
        if (m_hPipeIn != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hPipeIn);
            m_hPipeIn = INVALID_HANDLE_VALUE;
        }
        m_readerThread->wait(3000); // give it a reasonable timeout
        if (m_readerThread->isRunning()) {
            m_readerThread->terminate(); // last resort
            m_readerThread->wait();
        }
        delete m_readerThread;
        m_readerThread = nullptr;
    }

    // Close write pipe
    if (m_hPipeOut != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hPipeOut);
        m_hPipeOut = INVALID_HANDLE_VALUE;
    }

    // Terminate child process if still running
    if (m_pi.hProcess) {
        TerminateProcess(m_pi.hProcess, 1);
        WaitForSingleObject(m_pi.hProcess, 5000);
        CloseHandle(m_pi.hProcess);
        CloseHandle(m_pi.hThread);
        m_pi.hProcess = NULL;
        m_pi.hThread = NULL;
    }
}

bool PtyProcessWin::isRoot() const {
    // Could check for elevation here
    return false;
}

QString PtyProcessWin::foregroundProcessName() const { return QFileInfo(m_program).baseName(); }

void PtyProcessWin::onReadThreadData(const QByteArray &data) { emit readyRead(data); }
