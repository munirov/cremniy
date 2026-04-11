#pragma once

#include <functional>
#include <utility>

#include <QList>
#include <QMap>
#include <QString>

class ToolTab;
class FileDataBuffer;
class QWidget;

enum class ToolKind {
    FileTab,
    Window
};

enum class FileToolGroup {
    Always,
    Other
};

namespace FileToolOrder {
inline constexpr int Code = 100;
inline constexpr int Binary = 200;
}

class ToolsRegistry {
public:
    using TabCreator = std::function<ToolTab*(FileDataBuffer*)>;
    using WindowOpener = std::function<void(QWidget*)>;

    struct Descriptor {
        QString id;
        QString name;
        ToolKind kind = ToolKind::Window;
        FileToolGroup fileGroup = FileToolGroup::Other;
        bool autoOpen = false;
        int order = 0;
        TabCreator tabCreator;
        WindowOpener windowOpener;

        bool isValid() const;
    };

    static ToolsRegistry& instance();

    void registerTool(const Descriptor& descriptor);
    Descriptor descriptor(const QString& id) const;

    QList<Descriptor> availableTools() const;
    QList<Descriptor> availableFileTools() const;
    QList<Descriptor> availableFileTools(FileToolGroup group) const;
    QList<Descriptor> availableWindowTools() const;

    ToolTab* createFileTool(const QString& id, FileDataBuffer* buffer) const;
    bool openWindowTool(const QString& id, QWidget* parent) const;

private:
    QMap<QString, Descriptor> m_descriptors;
};

template <typename ToolTabType>
inline ToolsRegistry::Descriptor makeFileToolDescriptor(const QString& id,
    const QString& name,
    FileToolGroup group,
    bool autoOpen,
    int order)
{
    ToolsRegistry::Descriptor descriptor;
    descriptor.id = id;
    descriptor.name = name;
    descriptor.kind = ToolKind::FileTab;
    descriptor.fileGroup = group;
    descriptor.autoOpen = autoOpen;
    descriptor.order = order;
    descriptor.tabCreator = [](FileDataBuffer* buffer) -> ToolTab* {
        return new ToolTabType(buffer);
    };
    return descriptor;
}

template <typename ToolTabType>
inline bool registerFileTool(const QString& id,
    const QString& name,
    FileToolGroup group,
    bool autoOpen,
    int order)
{
    ToolsRegistry::instance().registerTool(
        makeFileToolDescriptor<ToolTabType>(id, name, group, autoOpen, order));
    return true;
}

template <typename ToolTabType>
inline bool registerAlwaysFileTool(const QString& id, const QString& name, int order)
{
    return registerFileTool<ToolTabType>(id, name, FileToolGroup::Always, true, order);
}

template <typename ToolTabType>
inline bool registerOtherFileTool(const QString& id, const QString& name)
{
    return registerFileTool<ToolTabType>(id, name, FileToolGroup::Other, false, 0);
}

template <typename Opener>
inline bool registerWindowTool(const QString& id,
    const QString& name,
    Opener&& opener)
{
    ToolsRegistry::Descriptor descriptor;
    descriptor.id = id;
    descriptor.name = name;
    descriptor.kind = ToolKind::Window;
    descriptor.windowOpener = std::forward<Opener>(opener);
    ToolsRegistry::instance().registerTool(descriptor);
    return true;
}
