#include <QMap>
#include <QString>
#include <functional>
#include "formatpagefactory.h"


FormatPageFactory& FormatPageFactory::instance() {
    static FormatPageFactory inst;
    return inst;
}

void FormatPageFactory::registerPage(const QString& id, Creator creator) {
    m_creators[id] = creator;
}

FormatPage* FormatPageFactory::create(const QString& id) {
    return m_creators.contains(id) ? m_creators[id]() : nullptr;
}

QStringList FormatPageFactory::availablePages() const {
    return m_creators.keys();
}