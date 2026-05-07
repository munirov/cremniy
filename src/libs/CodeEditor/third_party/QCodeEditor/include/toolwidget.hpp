#ifndef TOOLWIDGET_H
#define TOOLWIDGET_H

#include <qstringview.h>
class ToolWidget
{
public:
    virtual void setBData(const QByteArray& data) = 0;
    virtual QByteArray getBData() = 0;

};

#endif // TOOLWIDGET_H
