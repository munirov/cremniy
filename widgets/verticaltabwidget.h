#ifndef VERTICALTABWIDGET_H
#define VERTICALTABWIDGET_H

#include <QTabWidget>
#include "verticaltabstyle.h"

class VerticalTabWidget : public QTabWidget
{
public:
    VerticalTabWidget(QWidget* parent = nullptr)
        : QTabWidget(parent)
    {
        setTabBar(new VerticalTabBar());
        setTabPosition(QTabWidget::East);
    }
};

#endif // VERTICALTABWIDGET_H
