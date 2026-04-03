//
// Created by Dmitriy on 4/3/26.
//

#include "LanguageManager.h"

#include <QApplication>
#include <qlocale.h>
#include <QTranslator>

LanguageManager & LanguageManager::instance() {
    static LanguageManager inst;
    return inst;
}

void LanguageManager::setLocale(const QString& locale) {
    auto newTranslator = std::make_unique<QTranslator>();

    if (!newTranslator->load(QLocale(locale), "app", "_", translationsPath()))
        return;

    QApplication::removeTranslator(_m_translator.get());
    _m_translator = std::move(newTranslator);
    QApplication::installTranslator(_m_translator.get());

}

QString LanguageManager::translationsPath() {
    QString translationsPath = QApplication::applicationDirPath();

#if defined(Q_OS_APPLE) || defined(Q_OS_MAC)
    return translationsPath.append("/../Resources/translations/");
#else
    return translationsPath.append("/Resources/translations/");
#endif
}
