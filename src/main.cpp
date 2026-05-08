#include <QApplication>
#include <QCoreApplication>
#include <QImageReader>
#include <QDirIterator>
#include <QDebug>
#include <QResource>

#include "app/WelcomeWindow/welcomeform.h"
#include "core/locale/LanguageManager.h"

int main(int argc, char *argv[])
{
    #ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    #endif
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("Munirov");
    LanguageManager::instance().loadUserDefaultLocale();

    QCoreApplication::setOrganizationName("Munirov");
    QCoreApplication::setApplicationName("Cremniy");
    a.setWindowIcon(QIcon(":/icons/icon.svg"));

    // - - Themes - -

    // Icons
    Q_INIT_RESOURCE(phoicons);
    QIcon::setThemeSearchPaths({":/icons"});
    QIcon::setThemeName("phoicons");         // маленькими буквами!

    qDebug() << "=== SYSTEM DEBUG ===";
    qDebug() << "Supported formats:" << QImageReader::supportedImageFormats();
    qDebug() << "=== THEME DEBUG ===";
    qDebug() << "Theme Search Paths:" << QIcon::themeSearchPaths();
    qDebug() << "Current Theme Name:" << QIcon::themeName();

    // Переименовал в checkTheme, чтобы не было конфликта
    QFile checkTheme(":/icons/phoicons/index.theme");
    qDebug() << "index.theme exists in resources:" << checkTheme.exists();

    qDebug() << "=== RESOURCE TREE ===";
    QDirIterator it(":/icons", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qDebug() << "Found resource:" << it.next();
    }
    qDebug() << "====================";

    // Style
    QFile baseStyleFile(":/styles/base.qss");
    if (!baseStyleFile.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open the baseStyle file: " << baseStyleFile.errorString();
        return 1;
    }

    // Переименовал в qssThemeFile
    QFile qssThemeFile(":/styles/dark.qss");
    if (!qssThemeFile.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open the theme file: " << qssThemeFile.errorString();
        return 1;
    }

    QString baseStyle = QLatin1String(baseStyleFile.readAll());
    QString themeData = QLatin1String(qssThemeFile.readAll());

    baseStyleFile.close();
    qssThemeFile.close(); // Теперь закрываем правильный файл

    a.setStyleSheet(baseStyle + "\n" + themeData);

    WelcomeForm wf;
    wf.show();
    return QCoreApplication::exec();
}
