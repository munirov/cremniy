#include "settingsregistry.h"

#include <algorithm>

#include <QDebug>
#include <QSet>

SettingsRegistry& SettingsRegistry::instance()
{
    static SettingsRegistry registry;
    return registry;
}

bool SettingsRegistry::registerPage(SettingsPageDescriptor descriptor)
{
    if (descriptor.categoryId.isEmpty() || descriptor.pageId.isEmpty()
        || !descriptor.categoryTitle || !descriptor.pageTitle || !descriptor.createPage) {
        qWarning() << "Ignoring an incomplete settings page descriptor";
        return false;
    }

    const auto duplicate = std::find_if(
        m_pages.cbegin(),
        m_pages.cend(),
        [&descriptor](const SettingsPageDescriptor& page) {
            return page.pageId == descriptor.pageId;
        }
    );
    if (duplicate != m_pages.cend()) {
        qWarning() << "Ignoring duplicate settings page:" << descriptor.pageId;
        return false;
    }

    m_pages.append(std::move(descriptor));
    return true;
}

bool SettingsRegistry::registerModulePage(const QString& moduleId, SettingsPageDescriptor descriptor)
{
    if (moduleId.trimmed().isEmpty()) {
        qWarning() << "Ignoring settings page without a module owner";
        return false;
    }

    descriptor.ownerId = moduleId;
    return registerPage(std::move(descriptor));
}

QVector<SettingsPageDescriptor> SettingsRegistry::pages() const
{
    auto result = m_pages;
    std::sort(
        result.begin(),
        result.end(),
        [](const SettingsPageDescriptor& left, const SettingsPageDescriptor& right) {
            if (left.categoryOrder != right.categoryOrder)
                return left.categoryOrder < right.categoryOrder;
            if (left.categoryId != right.categoryId)
                return left.categoryId < right.categoryId;
            if (left.pageOrder != right.pageOrder)
                return left.pageOrder < right.pageOrder;
            return left.pageId < right.pageId;
        }
    );
    return result;
}

QStringList SettingsRegistry::moduleOptionKeys() const
{
    QStringList keys;
    QSet<QString> seen;
    for (const SettingsPageDescriptor& page : m_pages) {
        for (const ModuleOptionDescriptor& option : page.moduleOptions) {
            const QString key = option.key.trimmed();
            if (key.isEmpty() || seen.contains(key))
                continue;
            seen.insert(key);
            keys.append(key);
        }
    }
    return keys;
}
