#ifndef FORMATPAGEFACTORY_H
#define FORMATPAGEFACTORY_H

#include "formatpage.h"
class FormatPageFactory
{
public:
    using Creator = std::function<FormatPage*()>;

    static FormatPageFactory& instance();

    void registerPage(const QString& id, Creator creator);
    FormatPage* create(const QString& id);
    QStringList availablePages() const;

private:
    QMap<QString, Creator> m_creators;
};

#endif // FORMATPAGEFACTORY_H
