#ifndef VIEWMENU_H
#define VIEWMENU_H

#include "ui/MenuBar/basemenu.h"

class ViewMenu : public BaseMenu
{
    Q_OBJECT
private:
    QAction* m_wordWrap;
    QAction* m_tabReplace;
    QAction* m_tabWidth2;
    QAction* m_tabWidth4;
    QAction* m_tabWidth8;
    QAction* m_terminal;
    QAction* m_fileTree;
public:
    ViewMenu();
    void setupConnections(IDEWindow* ideWind);
};

#endif // VIEWMENU_H
