//
// Created by Dmitriy on 4/3/26.
//

#ifndef CREMNIY_LANGUAGE_MANAGER_H
#define CREMNIY_LANGUAGE_MANAGER_H

#include <QTranslator>

#include "config.h"


class LanguageManager {
private:
   std::unique_ptr<QTranslator> _m_translator;

public:
    static LanguageManager& instance();

    void setLocale(const QString& locale);

    static QString translationsPath();

    QStringList supportedLanguages() {
        return QStringLiteral(APP_LANGUAGES).split(';', Qt::SkipEmptyParts);
    }
};



#endif //CREMNIY_LANGUAGE_MANAGER_H
