#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QIcon>

#include "app/WelcomeWindow/welcomeform.h"
#include "locale/LanguageManager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("Munirov");
    LanguageManager::instance().setLocale("en");

    QCoreApplication::setOrganizationName("Munirov");
    QCoreApplication::setApplicationName("Cremniy");
    a.setWindowIcon(QIcon(":/icons/icon.svg"));

    // - - Themes - -

    // Icons
    QIcon::setThemeSearchPaths({":/icons"});
    QIcon::setThemeName("phoicons");         // маленькими буквами!

    // Style
    QFile baseStyleFile(":/styles/base.qss");
    if (!baseStyleFile.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open the baseStyle file: " << baseStyleFile.errorString();
        return 1;
    }

    QFile themeFile(":/styles/dark.qss");
    if (!themeFile.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open the theme file: " << themeFile.errorString();
        return 1;
    }

    QString baseStyle   = QLatin1String(baseStyleFile.readAll());
    QString theme  = QLatin1String(themeFile.readAll());

    baseStyleFile.close();
    baseStyleFile.close();

    a.setStyleSheet(baseStyle + "\n" + theme);

    WelcomeForm wf;
    wf.show();
    return QCoreApplication::exec();
}