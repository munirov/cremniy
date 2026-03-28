#include "toolsmenu.h"
#include "dialogs/reversecalculatordialog.h"
#include "ui/MenuBar/menufactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("5", [](){
        return new ToolsMenu();
    });
    return true;
}();

ToolsMenu::ToolsMenu() : BaseMenu("Tools") {
    m_reverseCalculator = new QAction("Reverse Calculator", this);
    this->addAction(m_reverseCalculator);
}

void ToolsMenu::setupConnections(IDEWindow* ideWind){
    connect(m_reverseCalculator, &QAction::triggered, this, &ToolsMenu::on_Open_ReverseCalculator);
}

void ToolsMenu::on_Open_ReverseCalculator()
{
    auto *dlg = new ReverseCalculatorDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose, true);
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}