#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <KodoTerm/KodoTerm.hpp>

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    void applyTheme(bool isDark);

private:
    void startShell(); // Метод для отложенного запуска
    
    KodoTerm *m_terminal;
    bool m_isStarted = false; 

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
};

#endif