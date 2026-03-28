#include "menubarbuilder.h"
#include "menufactory.h"

MenuBarBuilder::MenuBarBuilder(QMenuBar* mBar, IDEWindow *ideWind) :
    m_menuBar(mBar)
{
    auto& menuFactory = MenuFactory::instance();
    qDebug() << menuFactory.availableMenus();
    for (const QString& menuID : menuFactory.availableMenus()){
        BaseMenu* menu = menuFactory.create(menuID);
        menu->setupConnections(ideWind);
        m_menuBar->addMenu(menu);
    }
}
