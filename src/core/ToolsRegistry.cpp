#include "ToolsRegistry.h"

#include "ui/ToolsTabWidget/ToolTab.h"

#include <algorithm>

bool ToolsRegistry::Descriptor::isValid() const
{
    if (id.isEmpty()) {
        return false;
    }

    if (kind == ToolKind::FileTab) {
        return static_cast<bool>(tabCreator);
    }

    return static_cast<bool>(windowOpener);
}

ToolsRegistry& ToolsRegistry::instance()
{
    static ToolsRegistry inst;
    return inst;
}

void ToolsRegistry::registerTool(const Descriptor& descriptor)
{
    if (!descriptor.isValid()) {
        return;
    }

    m_descriptors[descriptor.id] = descriptor;
}

ToolsRegistry::Descriptor ToolsRegistry::descriptor(const QString& id) const
{
    return m_descriptors.value(id);
}

QList<ToolsRegistry::Descriptor> ToolsRegistry::availableTools() const
{
    QList<Descriptor> tools = m_descriptors.values();
    std::sort(tools.begin(), tools.end(), [](const Descriptor& left, const Descriptor& right) {
        if (left.kind != right.kind) {
            return left.kind == ToolKind::FileTab;
        }

        if (left.kind == ToolKind::FileTab && left.fileGroup != right.fileGroup) {
            return left.fileGroup == FileToolGroup::Always;
        }

        if (left.kind == ToolKind::FileTab && left.fileGroup == FileToolGroup::Always && left.order != right.order) {
            return left.order < right.order;
        }

        return left.name < right.name;
    });
    return tools;
}

QList<ToolsRegistry::Descriptor> ToolsRegistry::availableFileTools() const
{
    QList<Descriptor> tools;
    for (const Descriptor& descriptor : availableTools()) {
        if (descriptor.kind == ToolKind::FileTab) {
            tools.append(descriptor);
        }
    }
    return tools;
}

QList<ToolsRegistry::Descriptor> ToolsRegistry::availableFileTools(FileToolGroup group) const
{
    QList<Descriptor> tools;
    for (const Descriptor& descriptor : availableFileTools()) {
        if (descriptor.fileGroup == group) {
            tools.append(descriptor);
        }
    }
    return tools;
}

QList<ToolsRegistry::Descriptor> ToolsRegistry::availableWindowTools() const
{
    QList<Descriptor> tools;
    for (const Descriptor& descriptor : availableTools()) {
        if (descriptor.kind == ToolKind::Window) {
            tools.append(descriptor);
        }
    }
    return tools;
}

ToolTab* ToolsRegistry::createFileTool(const QString& id, FileDataBuffer* buffer) const
{
    const auto it = m_descriptors.constFind(id);
    if (it == m_descriptors.cend() || it->kind != ToolKind::FileTab) {
        return nullptr;
    }

    return it->tabCreator(buffer);
}

bool ToolsRegistry::openWindowTool(const QString& id, QWidget* parent) const
{
    const auto it = m_descriptors.constFind(id);
    if (it == m_descriptors.cend() || it->kind != ToolKind::Window) {
        return false;
    }

    it->windowOpener(parent);
    return true;
}
