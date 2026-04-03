#include "referencewindowfactory.h"

ReferenceWindowFactory& ReferenceWindowFactory::instance() {
    static ReferenceWindowFactory inst;
    return inst;
}

void ReferenceWindowFactory::registerRefWin(const QString& id, Creator creator) {
    m_creators[id] = creator;
}

ReferenceWindow* ReferenceWindowFactory::create(const QString& id) {
    return m_creators.contains(id) ? m_creators[id]() : nullptr;
}

QStringList ReferenceWindowFactory::availableRefWins() const {
    return m_creators.keys();
}
