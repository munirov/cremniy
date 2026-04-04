#ifndef THEMESERVICE_H
#define THEMESERVICE_H

#include <QString>
#include <QVector>

class ThemeService
{
public:
    struct BuiltinTheme {
        QString displayName;
        QString resourcePath;
    };

    static QVector<BuiltinTheme> builtinThemes();
    static QString defaultResourcePath();

    static bool applyStylesheet(const QString &pathOrQrc, QString *error = nullptr);
    static void applyFromSettings();
};

#endif // THEMESERVICE_H
