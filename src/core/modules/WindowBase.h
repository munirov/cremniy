#ifndef WINDOWBASE_H
#define WINDOWBASE_H

#include <QObject>
#include <qdialog.h>

class WindowBase : public QDialog {
    Q_OBJECT

private:


public:
    explicit WindowBase(QWidget* parent = nullptr) : QDialog(parent){}

public slots:
    void showWindow() { this->show(); }

};

#endif // WINDOWBASE_H
