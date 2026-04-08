#include "ToolTabFactory.h"

#include <algorithm>

ToolTabFactory& ToolTabFactory::instance() {
    static ToolTabFactory inst;
    return inst;
}

void ToolTabFactory::registerTab(const Descriptor& descriptor) {
    if (!descriptor.isValid()) {
        return;
    }

    m_descriptors[descriptor.id] = descriptor;
}

ToolTab* ToolTabFactory::create(const QString& id, FileDataBuffer* buffer) {
    const auto it = m_descriptors.constFind(id);
    return it != m_descriptors.cend() ? it->creator(buffer) : nullptr;
}

ToolTabFactory::Descriptor ToolTabFactory::descriptor(const QString& id) const {
    return m_descriptors.value(id);
}

QList<ToolTabFactory::Descriptor> ToolTabFactory::availableTabs() const {
    QList<Descriptor> tabs = m_descriptors.values();
    std::sort(tabs.begin(), tabs.end(), [](const Descriptor& left, const Descriptor& right) {
        if (left.group != right.group) {
            return left.group == ToolTabGroup::Always;
        }

        if (left.group == ToolTabGroup::Always && left.order != right.order) {
            return left.order < right.order;
        }

        return left.name < right.name;
    });
    return tabs;
}

QList<ToolTabFactory::Descriptor> ToolTabFactory::availableTabs(ToolTabGroup group) const {
    QList<Descriptor> tabs;
    for (const Descriptor& descriptor : availableTabs()) {
        if (descriptor.group == group) {
            tabs.append(descriptor);
        }
    }
    return tabs;
}
