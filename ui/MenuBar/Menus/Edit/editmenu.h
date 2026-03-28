#ifndef EDITMENU_H
#define EDITMENU_H

#include "ui/MenuBar/basemenu.h"

class EditMenu : public BaseMenu
{
    Q_OBJECT
private:
    QAction* m_settings;
public:
    EditMenu();
    void setupConnections(IDEWindow* ideWind);
};

#endif // EDITMENU_H
