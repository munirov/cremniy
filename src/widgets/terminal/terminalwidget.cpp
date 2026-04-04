#include "terminalwidget.h"
#include <QKeyEvent>
#include <QTextCursor>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QDebug>
#include <QVBoxLayout>
#include <QProcessEnvironment>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

// Вспомогательная функция для очистки текста (из ветки dev)
static QString stripAnsiCodes(const QString &text) {
    if (text.isEmpty()) return text;
    static QRegularExpression ansiRegex(
        "(\x1b\\][0-9];.*?\x07|"       // OSC (Operating System Commands)
        "\x1b\\[[0-9;?]*[A-Za-z])"      // CSI (Control Sequence Introducer)
    );

    QString cleaned = text;
    cleaned.remove(ansiRegex);
    return cleaned;
}

// Объединенный конструктор: поддерживаем рабочую директорию и темы
TerminalWidget::TerminalWidget(QWidget *parent, const QString &workingDirectory)
    : QWidget(parent), m_workingDirectory(workingDirectory)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Инициализация дисплея (Theme-friendly)
    m_display = new QAnsiTextEdit(this); 
    m_display->setObjectName("terminalDisplay");
    // Оставляем только шрифт, цвета придут из QSS
    m_display->setStyleSheet("font-family: 'Consolas', 'DejaVu Sans Mono', monospace; font-size: 10pt;");
    layout->addWidget(m_display);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, &TerminalWidget::onReadyRead);
    connect(m_process, &QProcess::errorOccurred, this, &TerminalWidget::onProcessError);

    loadHistory(); 
    setupShell();
    
    m_display->installEventFilter(this);
}

void TerminalWidget::setupShell() {
    QString workingDirectory = m_workingDirectory;
    if (!workingDirectory.isEmpty() && QDir(workingDirectory).exists()) {
        m_process->setWorkingDirectory(workingDirectory);
    }

#ifdef Q_OS_WIN
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Настройки для корректного отображения цветов в Windows PowerShell
    env.insert("TERM", "xterm-256color");
    env.insert("COLORTERM", "truecolor");
    m_process->setProcessEnvironment(env);
    m_process->start("powershell.exe", QStringList() << "-NoLogo" << "-NoExit" << "-Command" << "$env:TERM='xterm-256color'; [Console]::OutputEncoding = [System.Text.Encoding]::UTF8; chcp 65001; clear");
#else
    m_process->start("/bin/bash", QStringList() << "-i");
#endif
}

void TerminalWidget::onReadyRead() {
    QByteArray data = m_process->readAll();
    QString output = QString::fromUtf8(data); 
    
    m_display->moveCursor(QTextCursor::End);
    // Используем ANSI-парсер
    m_display->appendAnsiText(output); 
    
    m_lastPromptPos = m_display->toPlainText().length();
    m_display->moveCursor(QTextCursor::End);
}

bool TerminalWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_display && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        QTextCursor cursor = m_display->textCursor();

        // Обработка Ctrl+C
        if ((keyEvent->key() == Qt::Key_C) && (keyEvent->modifiers() & Qt::ControlModifier)) {
            if (cursor.hasSelection()) {
                return false; // Позволяем скопировать текст
            } else {
                if (m_process->state() == QProcess::Running) {
#ifdef Q_OS_WIN
                    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0);
                    m_process->write("\r\n");
#else
                    qint64 pid = m_process->processId();
                    if (pid > 0) {
                        kill(-pid, SIGINT);
                    }
#endif
                    m_display->moveCursor(QTextCursor::End);
                    m_display->insertPlainText("^C\n");
                    m_lastPromptPos = m_display->toPlainText().length();
                }
                return true;
            }
        }

        if (keyEvent->matches(QKeySequence::SelectAll)) {
            cursor.setPosition(m_lastPromptPos);
            cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            m_display->setTextCursor(cursor);
            return true;
        }

        bool isModifierOnly = (keyEvent->modifiers() != Qt::NoModifier && keyEvent->text().isEmpty());
        bool isNavigation = (keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right || 
                             keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down ||
                             keyEvent->key() == Qt::Key_Home || keyEvent->key() == Qt::Key_End);

        if (!isModifierOnly && !isNavigation) {
            if (cursor.position() < m_lastPromptPos || (cursor.hasSelection() && cursor.selectionStart() < m_lastPromptPos)) {
                cursor.movePosition(QTextCursor::End);
                m_display->setTextCursor(cursor);
            }
        }

        switch (keyEvent->key()) {
            case Qt::Key_Backspace:
                if (m_display->textCursor().position() <= m_lastPromptPos && !m_display->textCursor().hasSelection()) return true;
                break;
            case Qt::Key_Return:
            case Qt::Key_Enter:
                handleEnter();
                return true;
            case Qt::Key_Up:
                showHistory(-1);
                return true;
            case Qt::Key_Down:
                showHistory(1);
                return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TerminalWidget::loadHistory() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/terminal_history.txt";
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) m_history.append(line);
        }
        file.close();
    }
    m_historyIndex = m_history.size();
}

void TerminalWidget::saveHistory() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QFile file(dataDir + "/terminal_history.txt");
    
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        int start = qMax(0, m_history.size() - 100);
        for (int i = start; i < m_history.size(); ++i) {
            out << m_history[i] << "\n";
        }
        file.close();
    }
}

void TerminalWidget::showHistory(int direction) {
    if (m_history.isEmpty()) return;

    m_historyIndex += direction;
    if (m_historyIndex < 0) m_historyIndex = 0;
    if (m_historyIndex >= m_history.size()) {
        m_historyIndex = m_history.size();
        replaceCurrentCommand(""); 
        return;
    }
    
    replaceCurrentCommand(m_history[m_historyIndex]);
}

void TerminalWidget::replaceCurrentCommand(const QString &cmd) {
    QTextCursor cursor = m_display->textCursor();
    cursor.setPosition(m_lastPromptPos);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.insertText(cmd);
}

void TerminalWidget::handleEnter() {
    if (m_process->state() != QProcess::Running) return;

    QTextCursor cursor = m_display->textCursor();
    cursor.setPosition(m_lastPromptPos);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    
    QString cmd = cursor.selectedText().replace(QChar::ParagraphSeparator, '\n');
    
    if (!cmd.trimmed().isEmpty()) {
        if (m_history.isEmpty() || m_history.last() != cmd) {
            m_history.append(cmd);
            saveHistory();
        }
    }
    m_historyIndex = m_history.size();

    cursor.removeSelectedText(); 
    m_process->write((cmd + "\n").toUtf8());
}

void TerminalWidget::onProcessError(QProcess::ProcessError error) {
    m_display->appendPlainText("Terminal Error: " + QString::number(error));
}

TerminalWidget::~TerminalWidget() {
    saveHistory();

    if (m_process) {
        // Prevent QProcess from delivering signals into a partially destroyed widget.
        disconnect(m_process, nullptr, this, nullptr);
    }
    
    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(500);
    }
}