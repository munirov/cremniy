#include "editmenu.h"
#include "ui/MenuBar/menufactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("2", [](){
        return new EditMenu();
    });
    return true;
}();

EditMenu::EditMenu() : BaseMenu("Edit") {

    m_settings = new QAction("Settings", this);
    this->addSeparator();
    this->addAction(m_settings);
}

void EditMenu::setupConnections(IDEWindow* ideWind){
    connect(m_settings, &QAction::triggered, ideWind, &IDEWindow::on_openSettings);
}