#ifndef VIEWMENU_H
#define VIEWMENU_H

#include "ui/MenuBar/basemenu.h"

class ViewMenu : public BaseMenu
{
    Q_OBJECT
private:
    QAction* m_wordWrap;
    QAction* m_terminal;
public:
    ViewMenu();
    void setupConnections(IDEWindow* ideWind);
};

#endif // VIEWMENU_H
