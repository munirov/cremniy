#pragma once

#include "surfaceintegration.h"
#include "terminalview.h"

#include <QString>

namespace Cremniy::Terminal {
class PtyProcess;
}

class QKeyEvent;

class TerminalWidget final : public TerminalSolution::TerminalView,
                             private TerminalSolution::SurfaceIntegration
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr,
                            const QString &workingDirectory = QString());
    ~TerminalWidget() override;

    bool isRunning() const;

public slots:
    void restartShell();
    void stopShell();

signals:
    void processStarted(qint64 processId);
    void processFinished(int exitCode);
    void newTerminalRequested();
    void closeRequested();

protected:
    qint64 writeToPty(const QByteArray &data) override;
    bool resizePty(QSize size) override;
    void setClipboard(const QString &text) override;
    void contextMenuRequested(const QPoint &pos) override;
    std::optional<Link> toLink(const QString &text) override;
    void linkActivated(const Link &link) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void startShell();
    void writeStatus(const QString &message, bool error = false);

    void onOsc(int command, std::string_view text, bool initial, bool final) override;
    void onBell() override;
    void onTitle(const QString &title) override;
    void onSetClipboard(const QByteArray &text) override;
    void onGetClipboard() override;

    Cremniy::Terminal::PtyProcess *m_pty = nullptr;
    QString m_workingDirectory;
};
