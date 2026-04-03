#ifndef REFERENCEWINDOW_H
#define REFERENCEWINDOW_H

#include <qwidget.h>
class ReferenceWindow : public QWidget
{
    Q_OBJECT
public:

    virtual QString RefWinName() = 0;

private:
    virtual void initWindow() = 0;
    virtual void initWidgets() = 0;

public slots:
    void showWindow() { this->show(); }

};

#endif // REFERENCEWINDOW_H
