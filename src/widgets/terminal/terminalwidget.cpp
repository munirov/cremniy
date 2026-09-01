#include "terminalwidget.h"

#include "ptyprocess.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QMenu>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <array>

using Cremniy::Terminal::PtyProcess;
using Cremniy::Terminal::PtyProcessOptions;

namespace {

struct ShellCommand
{
    QString executable;
    QStringList arguments;
};

QFont applicationTerminalFont()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (QFontDatabase::hasFamily(QStringLiteral("JetBrains Mono")))
        font.setFamily(QStringLiteral("JetBrains Mono"));
    font.setPointSize(10);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    font.setWeight(QFont::Normal);
    return font;
}

ShellCommand defaultShell()
{
#ifdef Q_OS_WIN
    for (const QString &candidate : {QStringLiteral("pwsh.exe"),
                                     QStringLiteral("powershell.exe"),
                                     QStringLiteral("cmd.exe")}) {
        const QString executable = QStandardPaths::findExecutable(candidate);
        if (!executable.isEmpty()) {
            const bool powerShell = candidate.compare(QStringLiteral("cmd.exe"),
                                                       Qt::CaseInsensitive)
                                    != 0;
            return {executable, powerShell ? QStringList{QStringLiteral("-NoLogo")}
                                           : QStringList{}};
        }
    }
#else
    const QString configuredShell = qEnvironmentVariable("SHELL");
    if (QFileInfo(configuredShell).isExecutable())
        return {configuredShell, {}};
    for (const QString &candidate : {QStringLiteral("bash"), QStringLiteral("sh")}) {
        const QString executable = QStandardPaths::findExecutable(candidate);
        if (!executable.isEmpty())
            return {executable, {}};
    }
#endif
    return {};
}

std::array<QColor, 19> defaultTerminalColors()
{
    return {
        QColor("#000000"), QColor("#cd3131"), QColor("#0dbc79"), QColor("#e5e510"),
        QColor("#2472c8"), QColor("#bc3fbc"), QColor("#11a8cd"), QColor("#e5e5e5"),
        QColor("#666666"), QColor("#f14c4c"), QColor("#23d18b"), QColor("#f5f543"),
        QColor("#3b8eea"), QColor("#d670d6"), QColor("#29b8db"), QColor("#ffffff"),
        QColor("#cccccc"), QColor("#1e1e1e"), QColor("#264f78"),
    };
}

} // namespace

TerminalWidget::TerminalWidget(QWidget *parent, const QString &workingDirectory)
    : TerminalSolution::TerminalView(parent)
    , m_workingDirectory(QDir::cleanPath(workingDirectory))
{
    setObjectName(QStringLiteral("terminalView"));
    setFont(applicationTerminalFont());
    setColors(defaultTerminalColors());
    setSurfaceIntegration(this);
    enableMouseTracking(true);
    surface()->enableLiveReflow(true);

    m_pty = Cremniy::Terminal::createPtyProcess(this);
    connect(m_pty, &PtyProcess::dataReceived, this, [this](const QByteArray &data) {
        writeToTerminal(data, false);
    });
    connect(m_pty, &PtyProcess::started, this, [this](qint64 processId) {
        resizePty(surface()->liveSize());
        emit processStarted(processId);
    });
    connect(m_pty, &PtyProcess::finished, this, [this](int exitCode) {
        writeStatus(tr("Process exited with code %1.").arg(exitCode), exitCode != 0);
        emit processFinished(exitCode);
    });
    connect(m_pty, &PtyProcess::errorOccurred, this, [this](const QString &message) {
        writeStatus(message, true);
    });

    QTimer::singleShot(0, this, &TerminalWidget::startShell);
}

TerminalWidget::~TerminalWidget()
{
    m_pty->stop();
}

bool TerminalWidget::isRunning() const
{
    return m_pty->isRunning();
}

void TerminalWidget::restartShell()
{
    m_pty->stop();
    TerminalSolution::TerminalView::restart();
    startShell();
}

void TerminalWidget::stopShell()
{
    m_pty->stop();
}

qint64 TerminalWidget::writeToPty(const QByteArray &data)
{
    return m_pty && m_pty->isRunning() ? m_pty->write(data) : 0;
}

bool TerminalWidget::resizePty(QSize size)
{
    if (!m_pty || !m_pty->isRunning())
        return true;
    return m_pty->resize(size);
}

void TerminalWidget::setClipboard(const QString &text)
{
    QApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

void TerminalWidget::contextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);
    QAction *copy = menu.addAction(tr("Copy"), this, &TerminalWidget::copyToClipboard);
    copy->setEnabled(selection().has_value());
    menu.addAction(tr("Paste"), this, &TerminalWidget::pasteFromClipboard);
    menu.addAction(tr("Select All"), this, &TerminalWidget::selectAll);
    menu.addSeparator();
    menu.addAction(tr("Clear"), this, &TerminalWidget::clearContents);
    menu.addAction(tr("Restart Terminal"), this, &TerminalWidget::restartShell);
    menu.addSeparator();
    menu.addAction(tr("New Terminal"), this, [this] { emit newTerminalRequested(); });
    QAction *stop = menu.addAction(tr("Kill Terminal"), this, [this] { emit closeRequested(); });
    stop->setEnabled(isRunning());
    menu.exec(viewport()->mapToGlobal(pos));
}

std::optional<TerminalWidget::Link> TerminalWidget::toLink(const QString &text)
{
    const QUrl url = QUrl::fromUserInput(text);
    if (url.isValid() && (url.scheme() == QStringLiteral("http")
                          || url.scheme() == QStringLiteral("https"))) {
        return Link{text};
    }
    return std::nullopt;
}

void TerminalWidget::linkActivated(const Link &link)
{
    QDesktopServices::openUrl(QUrl::fromUserInput(link.text));
}

void TerminalWidget::keyPressEvent(QKeyEvent *event)
{
    const bool ctrlShift = event->modifiers().testFlag(Qt::ControlModifier)
                           && event->modifiers().testFlag(Qt::ShiftModifier);
    if ((event->matches(QKeySequence::Copy) || (ctrlShift && event->key() == Qt::Key_C))
        && selection().has_value()) {
        copyToClipboard();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste) || (ctrlShift && event->key() == Qt::Key_V)) {
        pasteFromClipboard();
        event->accept();
        return;
    }
    if (event->modifiers() == Qt::ControlModifier
        && (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal)) {
        zoomIn();
        event->accept();
        return;
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Minus) {
        zoomOut();
        event->accept();
        return;
    }
    TerminalSolution::TerminalView::keyPressEvent(event);
}

void TerminalWidget::startShell()
{
    const ShellCommand shell = defaultShell();
    if (shell.executable.isEmpty()) {
        writeStatus(tr("No supported shell was found."), true);
        return;
    }

    const QString workingDirectory = QDir(m_workingDirectory).exists()
                                         ? m_workingDirectory
                                         : QDir::homePath();
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    environment.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    environment.insert(QStringLiteral("TERM_PROGRAM"),
                       QCoreApplication::applicationName());
    environment.insert(QStringLiteral("TERM_PROGRAM_VERSION"),
                       QCoreApplication::applicationVersion());

    PtyProcessOptions options;
    options.executable = shell.executable;
    options.arguments = shell.arguments;
    options.workingDirectory = workingDirectory;
    options.environment = environment;
    options.initialSize = surface()->liveSize();
    m_pty->start(options);
}

void TerminalWidget::writeStatus(const QString &message, bool error)
{
    const QByteArray color = error ? QByteArrayLiteral("\x1b[31m")
                                   : QByteArrayLiteral("\x1b[90m");
    writeToTerminal(QByteArrayLiteral("\r\n") + color + message.toUtf8()
                        + QByteArrayLiteral("\x1b[0m\r\n"),
                    true);
}

void TerminalWidget::onOsc(int, std::string_view, bool, bool)
{
}

void TerminalWidget::onBell()
{
    QApplication::beep();
}

void TerminalWidget::onTitle(const QString &title)
{
    Q_UNUSED(title)
}

void TerminalWidget::onSetClipboard(const QByteArray &)
{
    // OSC 52 writes are intentionally ignored. A child process must not be able
    // to replace the desktop clipboard without an explicit user action.
}

void TerminalWidget::onGetClipboard()
{
}
