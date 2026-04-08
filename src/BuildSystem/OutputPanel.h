#pragma once
#include <QDockWidget>
#include <QPlainTextEdit>

class OutputPanel : public QDockWidget {
    Q_OBJECT
public:
    explicit OutputPanel(QWidget* parent = nullptr);

public slots:
    void appendLine(const QString& line);
    void clear();

private:
    QPlainTextEdit* m_output;
};