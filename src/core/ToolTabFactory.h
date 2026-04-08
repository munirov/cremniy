#pragma once

#include <functional>
#include <QList>
#include <QMap>
#include <QString>

class ToolTab;
class FileDataBuffer;

enum class ToolTabGroup {
    Always,
    Other
};

namespace ToolTabOrder {
inline constexpr int Code = 100;
inline constexpr int Binary = 200;
}

class ToolTabFactory {

public:
    using Creator = std::function<ToolTab*(FileDataBuffer*)>;

    struct Descriptor {
        QString id;
        QString name;
        ToolTabGroup group = ToolTabGroup::Other;
        bool autoOpen = false;
        int order = 0;
        Creator creator;

        bool isValid() const { return !id.isEmpty() && static_cast<bool>(creator); }
    };

    static ToolTabFactory& instance();

    void registerTab(const Descriptor& descriptor);
    ToolTab* create(const QString& id, FileDataBuffer* buffer);
    Descriptor descriptor(const QString& id) const;
    QList<Descriptor> availableTabs() const;
    QList<Descriptor> availableTabs(ToolTabGroup group) const;

private:
    QMap<QString, Descriptor> m_descriptors;

};

template <typename ToolTabType>
inline ToolTabFactory::Descriptor makeToolTabDescriptor(const QString& id,
    const QString& name,
    ToolTabGroup group,
    bool autoOpen,
    int order)
{
    ToolTabFactory::Descriptor descriptor;
    descriptor.id = id;
    descriptor.name = name;
    descriptor.group = group;
    descriptor.autoOpen = autoOpen;
    descriptor.order = order;
    descriptor.creator = [](FileDataBuffer* buffer) -> ToolTab* {
        return new ToolTabType(buffer);
    };
    return descriptor;
}

template <typename ToolTabType>
inline bool registerToolTab(const QString& id,
    const QString& name,
    ToolTabGroup group,
    bool autoOpen,
    int order)
{
    ToolTabFactory::instance().registerTab(
        makeToolTabDescriptor<ToolTabType>(id, name, group, autoOpen, order));
    return true;
}

template <typename ToolTabType>
inline bool registerAlwaysToolTab(const QString& id, const QString& name, int order)
{
    return registerToolTab<ToolTabType>(id, name, ToolTabGroup::Always, true, order);
}

template <typename ToolTabType>
inline bool registerOtherToolTab(const QString& id, const QString& name)
{
    return registerToolTab<ToolTabType>(id, name, ToolTabGroup::Other, false, 0);
}
