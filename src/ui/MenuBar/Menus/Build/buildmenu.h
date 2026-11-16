#ifndef BUILDMENU_H
#define BUILDMENU_H

#include "ui/MenuBar/basemenu.h"

class BuildMenu : public BaseMenu
{
    Q_OBJECT
public:
    BuildMenu();
    void setupConnections(IDEWindow* ideWind);

private:
    QAction* m_build;
    QAction* m_openBuildConfigurator;
};

#endif // BUILDMENU_H
