#pragma once

#include <functional>
#include <QMap>
#include <QString>

class ToolTab;
class FileDataBuffer;

class ToolTabFactory {

public:
    using Creator = std::function<ToolTab*(FileDataBuffer*)>;

    static ToolTabFactory& instance();

    void registerTab(const QString& id, Creator creator);
    ToolTab* create(const QString& id, FileDataBuffer* buffer);
    QStringList availableTabs() const;

private:
    QMap<QString, Creator> m_creators;

};
