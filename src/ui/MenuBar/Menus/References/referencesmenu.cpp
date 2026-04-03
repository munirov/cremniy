#include "referencesmenu.h"
#include "core/modules/ModuleManager.h"
#include "ui/MenuBar/menufactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("6", [](){
        return new ReferencesMenu();
    });
    return true;
}();

ReferencesMenu::ReferencesMenu() : BaseMenu(tr("References")) {
    QList<QString> groups = ModuleManager::instance().getReferenceGroups();
    for (QString group : groups){

        const QVector<ReferenceModuleDescription>& descRefModules = ModuleManager::instance().getReferencesByGroup(group);

        QMenu* groupMenu;
        if (group == "") groupMenu = this;
        else groupMenu = new QMenu(group);

        for (const ReferenceModuleDescription& desc : descRefModules){
            QAction* newAction = new QAction(desc.name, this);
            groupMenu->addAction(newAction);
            connect(newAction, &QAction::triggered, this, [this, desc](){
                auto* module = desc.creator();
                module->showWindow();
            });
        }
        if (group != "") this->addMenu(groupMenu);

    }
}