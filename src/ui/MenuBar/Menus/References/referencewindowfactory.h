#ifndef REFERENCEWINDOWFACTORY_H
#define REFERENCEWINDOWFACTORY_H

#include "MenuBar/Menus/References/referencewindow.h"
class ReferenceWindowFactory
{

public:
    using Creator = std::function<ReferenceWindow*()>;

    static ReferenceWindowFactory& instance();

    void registerRefWin(const QString& id, Creator creator);
    ReferenceWindow* create(const QString& id);
    QStringList availableRefWins() const;

private:
    QMap<QString, Creator> m_creators;

};

#endif // REFERENCEWINDOWFACTORY_H
