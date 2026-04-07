#ifndef REFERENCESMENU_H
#define REFERENCESMENU_H

#include "ui/MenuBar/basemenu.h"

class ReferencesMenu : public BaseMenu
{
    Q_OBJECT
private:
    QAction* m_asciiChars;
    QAction* m_keybScancodes;
public:
    ReferencesMenu();
    void setupConnections(IDEWindow *ideWind) override;
};

#endif // REFERENCESMENU_H
