#ifndef MENUBARBUILDER_H
#define MENUBARBUILDER_H

#include <qmenubar.h>
#include "app/IDEWindow/idewindow.h"

class MenuBarBuilder
{

private:
    QMenuBar* m_menuBar;

public:
    MenuBarBuilder(QMenuBar* mBar, IDEWindow* ideWind);
};

#endif // MENUBARBUILDER_H
