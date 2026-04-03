#include "toolsmenu.h"
#include "ui/MenuBar/menufactory.h"
#include "core/modules/ModuleManager.h"
#include <QKeySequence>
#include <QAction>

#include "core/modules/WindowBase.h"

static bool registered = []() {
  MenuFactory::instance().registerMenu("5", []() { return new ToolsMenu(); });
  return true;
}();

ToolsMenu::ToolsMenu() : BaseMenu(tr("Tools")) {
    QMenu* tabModulesMenu = new QMenu(tr("Tabs"));
    QMenu* windowModulesMenu = new QMenu(tr("Windows"));

    // Modules: Tabs
    const QList<QString>& tabGroups = ModuleManager::instance().getGroups<TabBase>();

    for (const QString& group : tabGroups){

        if (group == "always") continue;

        const QVector<ModuleDescription<TabBase>>& creatorTabModules = ModuleManager::instance().getByGroup<TabBase>(group);

        QMenu* groupMenu;
        if (group == "") groupMenu = tabModulesMenu;
        else groupMenu = new QMenu(group);

        for (const ModuleDescription<TabBase>& desc : creatorTabModules){

            QAction* newAction = new QAction(desc.name(), this);
            groupMenu->addAction(newAction);

            connect(newAction, &QAction::triggered, this, [this, desc](){
                emit openTabModule(desc);
            });

        }

        if (group != "") tabModulesMenu->addMenu(groupMenu);

    }


    // Nodules: Windows
    const QList<QString>& windowGroups = ModuleManager::instance().getGroups<WindowBase>();

    for (const QString& group : windowGroups){

        const QVector<ModuleDescription<WindowBase>>& descWindowModules = ModuleManager::instance().getByGroup<WindowBase>(group);

        QMenu* groupMenu;
        if (group == "") groupMenu = windowModulesMenu;
        else groupMenu = new QMenu(group);

        for (const ModuleDescription<WindowBase>& desc : descWindowModules){

            QAction* newAction = new QAction(desc.name(), this);
            groupMenu->addAction(newAction);

            connect(newAction, &QAction::triggered, this, [this, desc](){
                auto* module = desc.creator();
                module->showWindow();
            });

        }

        if (group != "") windowModulesMenu->addMenu(groupMenu);

    }


    this->addMenu(tabModulesMenu);
    this->addMenu(windowModulesMenu);
}

void ToolsMenu::setupConnections(IDEWindow* ideWind){
    connect(this, &ToolsMenu::openTabModule, ideWind, &IDEWindow::openTabModule);
}