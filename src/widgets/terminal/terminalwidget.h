#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QProcess>
#include <QStringList>

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget();

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);

private:
    void setupShell();
    void handleEnter();
    
    /* History */
    void showHistory(int direction);
    void replaceCurrentCommand(const QString &cmd);
    void loadHistory();
    void saveHistory();
    
    bool eventFilter(QObject *obj, QEvent *event) override;

    QPlainTextEdit *m_display;
    QProcess *m_process;
    
    int m_lastPromptPos = 0;
    
    QStringList m_history;
    int m_historyIndex = 0;
};

#endif
