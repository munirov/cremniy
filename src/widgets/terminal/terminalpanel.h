#pragma once

#include <QString>
#include <QWidget>

class QTabWidget;
class QToolButton;
class TerminalWidget;

class TerminalPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(const QString &workingDirectory, QWidget *parent = nullptr);

    void focusActiveTerminal();

public slots:
    void newTerminal();
    void closeActiveTerminal();

signals:
    void closeRequested();

private:
    TerminalWidget *currentTerminal() const;
    void closeTerminalTab(int index);
    void updateActions();

    QString m_workingDirectory;
    QString m_projectName;
    QTabWidget *m_tabs = nullptr;
    QToolButton *m_closeButton = nullptr;
    int m_nextTerminalNumber = 1;
};
