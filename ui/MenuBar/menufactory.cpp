#include "menufactory.h"

MenuFactory& MenuFactory::instance() {
    static MenuFactory inst;
    return inst;
}

void MenuFactory::registerMenu(const QString& id, Creator creator) {
    m_creators[id] = creator;
}

BaseMenu* MenuFactory::create(const QString& id) {
    return m_creators.contains(id) ? m_creators[id]() : nullptr;
}

QStringList MenuFactory::availableMenus() const {
    return m_creators.keys();
}
