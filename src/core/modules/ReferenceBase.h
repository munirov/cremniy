#ifndef REFERENCEWINDOW_H
#define REFERENCEWINDOW_H

#include <qwidget.h>

class ReferenceBase : public QWidget {
    Q_OBJECT
public:

private:
    virtual void initWindow() = 0;
    virtual void initWidgets() = 0;

public slots:
    virtual void showWindow() { this->show(); }

};

#endif // REFERENCEWINDOW_H
