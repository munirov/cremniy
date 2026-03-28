#ifndef MENUFACTORY_H
#define MENUFACTORY_H

#include "ui/MenuBar/basemenu.h"

class MenuFactory
{
public:
    using Creator = std::function<BaseMenu*()>;

    static MenuFactory& instance();

    void registerMenu(const QString& id, Creator creator);
    BaseMenu* create(const QString& id);
    QStringList availableMenus() const;

private:
    QMap<QString, Creator> m_creators;
};

#endif // MENUFACTORY_H
