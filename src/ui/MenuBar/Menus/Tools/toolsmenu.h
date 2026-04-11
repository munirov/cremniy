#ifndef TOOLSMENU_H
#define TOOLSMENU_H

#include "ui/MenuBar/basemenu.h"

#include <QList>

class IDEWindow;
class QAction;

class ToolsMenu : public BaseMenu
{
    Q_OBJECT

private:
    IDEWindow* m_ideWindow = nullptr;
    QList<QAction*> m_toolTabActions;
    QList<QAction*> m_toolWindowActions;

public:
    ToolsMenu();
    void setupConnections(IDEWindow* ideWind);
};

#endif // TOOLSMENU_H
