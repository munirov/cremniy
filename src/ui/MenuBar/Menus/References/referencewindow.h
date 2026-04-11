#ifndef REFERENCEWINDOW_H
#define REFERENCEWINDOW_H

#include <qwidget.h>
class ReferenceWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ReferenceWindow(QWidget *parent = nullptr) : QWidget(parent) {}

    virtual QString RefWinName() = 0;

private:
    virtual void initWindow() = 0;
    virtual void initWidgets() = 0;

public slots:
    virtual void showWindow() { this->show(); }

};

#endif // REFERENCEWINDOW_H
