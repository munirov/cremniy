#include "toolsmenu.h"
#include "ui/MenuBar/menufactory.h"
#include "core/modules/ModuleManager.h"
#include <QKeySequence>
#include <QAction>

static bool registered = []() {
  MenuFactory::instance().registerMenu("5", []() { return new ToolsMenu(); });
  return true;
}();

ToolsMenu::ToolsMenu() : BaseMenu(tr("Tools")) {
    QMenu* tabModulesMenu = new QMenu(tr("Tabs"));
    QMenu* windowModulesMenu = new QMenu(tr("Windows"));

    // Modules: Tabs
    const QList<QString>& tabGroups = ModuleManager::instance().getTabGroups();

    for (const QString& group : tabGroups){

        if (group == "always") continue;

        const QVector<TabModuleDescription>& creatorTabModules = ModuleManager::instance().getTabsByGroup(group);

        QMenu* groupMenu;
        if (group == "") groupMenu = tabModulesMenu;
        else groupMenu = new QMenu(group);

        for (const TabModuleDescription& desc : creatorTabModules){

            QAction* newAction = new QAction(desc.name, this);
            groupMenu->addAction(newAction);

            connect(newAction, &QAction::triggered, this, [this, desc](){
                emit openTabModule(desc);
            });

        }

        if (group != "") tabModulesMenu->addMenu(groupMenu);

    }


    // Nodules: Windows
    const QList<QString>& windowGroups = ModuleManager::instance().getWindowGroups();

    for (const QString& group : windowGroups){

        const QVector<WindowModuleDescription>& descWindowModules = ModuleManager::instance().getWindowsByGroup(group);

        QMenu* groupMenu;
        if (group == "") groupMenu = windowModulesMenu;
        else groupMenu = new QMenu(group);

        for (const WindowModuleDescription& desc : descWindowModules){

            QAction* newAction = new QAction(desc.name, this);
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