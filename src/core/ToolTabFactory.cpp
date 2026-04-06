#include "ToolTabFactory.h"

ToolTabFactory& ToolTabFactory::instance() {
    static ToolTabFactory inst;
    return inst;
}

void ToolTabFactory::registerTab(const QString& id, Creator creator) {
    m_creators[id] = creator;
}

ToolTab* ToolTabFactory::create(const QString& id, FileDataBuffer* buffer) {
    return m_creators.contains(id) ? m_creators[id](buffer) : nullptr;
}

QStringList ToolTabFactory::availableTabs() const {
    return m_creators.keys();
}