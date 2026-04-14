#include "referencesmenu.h"
#include "ui/MenuBar/menufactory.h"
#include "core/ToolsRegistry.h"
#include <QAction>

static bool registered = [](){
    MenuFactory::instance().registerMenu("6", [](){
        return new ReferencesMenu();
    });
    return true;
}();

ReferencesMenu::ReferencesMenu() : BaseMenu("References") {
    const auto descriptors = ToolsRegistry::instance().availableReferenceTools();
    for (const auto& descriptor : descriptors){
        QAction* act = new QAction(descriptor.name, this);
        act->setProperty("toolId", descriptor.id);
        this->addAction(act);
        m_toolActions.append(act);
    }
}

void ReferencesMenu::setupConnections(IDEWindow* ideWind) {
    for (QAction* action : m_toolActions) {
        connect(action, &QAction::triggered, this, [action, ideWind]() {
            ToolsRegistry::instance().openWindowTool(action->property("toolId").toString(), (QWidget*)ideWind);
        });
    }
}