#include "viewmenu.h"
#include "ui/MenuBar/menufactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("3", [](){
        return new ViewMenu();
    });
    return true;
}();

ViewMenu::ViewMenu() : BaseMenu(tr("View")) {
    m_wordWrap = new QAction(tr("Word Wrap"), this);
    m_wordWrap->setCheckable(true);
    m_wordWrap->setChecked(true);

    m_terminal = new QAction(tr("Show terminal"), this);
    m_terminal->setCheckable(true);
    m_terminal->setChecked(true);

    this->addAction(m_wordWrap);
    this->addSeparator();
    this->addAction(m_terminal);
}

void ViewMenu::setupConnections(IDEWindow* ideWind){
    connect(m_terminal, &QAction::triggered, ideWind, &IDEWindow::on_Toggle_Terminal);
}
