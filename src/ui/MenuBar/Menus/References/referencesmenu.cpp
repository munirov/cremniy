#include "referencesmenu.h"
#include "ui/MenuBar/menufactory.h"
#include "ui/MenuBar/Menus/References/referencewindowfactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("6", [](){
        return new ReferencesMenu();
    });
    return true;
}();

ReferencesMenu::ReferencesMenu() : BaseMenu("References") {
    auto& RefWinFactory = ReferenceWindowFactory::instance();
    qDebug() << RefWinFactory.availableRefWins();
    for (const QString& RefWinID : RefWinFactory.availableRefWins()){
        ReferenceWindow* refWin = RefWinFactory.create(RefWinID);
        QAction* act = new QAction(refWin->RefWinName(), this);
        this->addAction(act);
        connect(act, &QAction::triggered, refWin, &ReferenceWindow::showWindow);
    }

}