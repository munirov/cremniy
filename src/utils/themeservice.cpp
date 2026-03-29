#include "themeservice.h"

#include "utils/appsettings.h"

#include <QApplication>
#include <QFile>

QVector<ThemeService::BuiltinTheme> ThemeService::builtinThemes()
{
    return {
        {QObject::tr("Cremniy Dark"), QStringLiteral(":/styles/dark.qss")},
        {QObject::tr("Cremniy Light"), QStringLiteral(":/styles/light.qss")},
    };
}

QString ThemeService::defaultResourcePath()
{
    return QStringLiteral(":/styles/dark.qss");
}

bool ThemeService::applyStylesheet(const QString &pathOrQrc, QString *error)
{
    if (pathOrQrc.trimmed().isEmpty()) {
        if (error)
            *error = QObject::tr("Empty theme path");
        return false;
    }

    const QString p = pathOrQrc.trimmed();
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Cannot open stylesheet: %1").arg(p);
        return false;
    }

    const QByteArray data = f.readAll();
    qApp->setStyleSheet(QString::fromUtf8(data));
    return true;
}

void ThemeService::applyFromSettings()
{
    const QString p = AppSettings::themeQssPath();
    QString err;
    if (!applyStylesheet(p, &err))
        applyStylesheet(defaultResourcePath(), nullptr);
}
