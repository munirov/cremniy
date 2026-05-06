//
// Created by Dmitriy on 4/3/26.
//

#include "LanguageManager.h"

#include <QApplication>
#include <qlocale.h>
#include <QTranslator>

#include "appsettings.h"

LanguageManager & LanguageManager::instance() {
    static LanguageManager inst;
    return inst;
}

void LanguageManager::loadUserDefaultLocale() {
    const QJsonObject settings = AppSettings::getSettingsJson();
    if (settings["language"].isNull() || settings["language"] == "")
        setLocale("en");
    else
        setLocale(settings["language"].toString());
}

void LanguageManager::setLocale(const QString& locale) {
    auto base_qt = std::make_unique<QTranslator>();
    auto newTranslator = std::make_unique<QTranslator>();

    QApplication::removeTranslator(_m_base_translator.get());
    QApplication::removeTranslator(_m_translator.get());

    if (base_qt->load(QLocale(locale), "qtbase", "_", translationsPath())){
        _m_base_translator = std::move(base_qt);
        QApplication::installTranslator(_m_base_translator.get());
    }
    if (newTranslator->load(QLocale(locale), "app", "_", translationsPath())){
        _m_translator = std::move(newTranslator);
        QApplication::installTranslator(_m_translator.get());
    }

    QJsonObject settings = AppSettings::getSettingsJson();
    settings["language"] = locale;
    AppSettings::updateSettingsJson(settings);
}

QString LanguageManager::translationsPath() {
    QString translationsPath = QApplication::applicationDirPath();

#if defined(Q_OS_APPLE) || defined(Q_OS_MAC)
    return translationsPath.append("/../Resources/translations/");
#else
    return translationsPath.append("/Resources/translations/");
#endif
}
