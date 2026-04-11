#include "toolsmenu.h"


#include "core/ToolsRegistry.h"
#include "ui/MenuBar/menufactory.h"

#include <QAction>
#include <QMenu>

static bool registered = []() {
    MenuFactory::instance().registerMenu("5", []() { return new ToolsMenu(); });
    return true;
}();

ToolsMenu::ToolsMenu()
    : BaseMenu("Tools")
{
    QMenu* fileToolsMenu = addMenu(tr("File Tools"));
    const auto toolTabDescriptors = ToolsRegistry::instance().availableFileTools(FileToolGroup::Other);
    for (const auto& descriptor : toolTabDescriptors) {
        auto* action = new QAction(descriptor.name, fileToolsMenu);
        action->setProperty("toolTabId", descriptor.id);
        m_toolTabActions.append(action);
        fileToolsMenu->addAction(action);
    }

    QMenu* windowToolsMenu = addMenu(tr("Window Tools"));
    const auto toolWindowDescriptors = ToolsRegistry::instance().availableWindowTools();
    for (const auto& descriptor : toolWindowDescriptors) {
        auto* action = new QAction(descriptor.name, windowToolsMenu);
        action->setProperty("toolWindowId", descriptor.id);
        m_toolWindowActions.append(action);
        windowToolsMenu->addAction(action);
    }
}

void ToolsMenu::setupConnections(IDEWindow* ideWind)
{
    m_ideWindow = ideWind;

    for (QAction* action : m_toolTabActions) {
        connect(action, &QAction::triggered, this, [this, action]() {
            if (!m_ideWindow) {
                return;
            }

            m_ideWindow->openToolForCurrentFile(action->property("toolTabId").toString());
        });
    }

    for (QAction* action : m_toolWindowActions) {
        connect(action, &QAction::triggered, this, [this, action]() {
            ToolsRegistry::instance().openWindowTool(action->property("toolWindowId").toString(), m_ideWindow);
        });
    }
}
