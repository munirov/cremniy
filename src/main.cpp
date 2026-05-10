#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDirIterator>
#include <QImageReader>
#include <QResource>
#include <QFontDatabase>

#include "app/WelcomeWindow/welcomeform.h"
#include "core/locale/LanguageManager.h"

int main(int argc, char* argv[]) {
#ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
#endif
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("Munirov");
    QCoreApplication::setApplicationName("Cremniy");

    LanguageManager::instance().loadUserDefaultLocale();

    a.setWindowIcon(QIcon(":/icons/icon.svg"));

    // - - Fonts - -

    int jbFontRegId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");
    int jbFontBoldId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Bold.ttf");
    int jbFontItalId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Italic.ttf");

    QString jbFontFamily = QFontDatabase::applicationFontFamilies(jbFontRegId).at(0);

    qDebug() << jbFontFamily;

    // - - Themes - -

    // Icons
    Q_INIT_RESOURCE(phoicons);
    QIcon::setThemeSearchPaths({":/icons"});
    QIcon::setThemeName("phoicons");

    qDebug() << "=== SYSTEM DEBUG ===";
    qDebug() << "Supported formats:" << QImageReader::supportedImageFormats();
    qDebug() << "=== THEME DEBUG ===";
    qDebug() << "Theme Search Paths:" << QIcon::themeSearchPaths();
    qDebug() << "Current Theme Name:" << QIcon::themeName();

    QFile checkTheme(":/icons/phoicons/index.theme");
    qDebug() << "index.theme exists in resources:" << checkTheme.exists();

    qDebug() << "=== RESOURCE TREE ===";
    QDirIterator it(":/icons", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qDebug() << "Found resource:" << it.next();
    }
    qDebug() << "====================";

    // Style
    QApplication::setStyle("Fusion");

    QFile baseStyleFile(":/styles/base.qss");
    if (!baseStyleFile.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open the baseStyle file: " << baseStyleFile.errorString();
        return 1;
    }

    QFile qssThemeFile(":/styles/dark.qss");
    if (!qssThemeFile.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open the theme file: " << qssThemeFile.errorString();
        return 1;
    }

    QString baseStyle = QLatin1String(baseStyleFile.readAll());
    QString themeData = QLatin1String(qssThemeFile.readAll());

    baseStyleFile.close();
    qssThemeFile.close();

    a.setStyleSheet(baseStyle + "\n" + themeData);

    WelcomeForm wf;
    wf.show();
    return QCoreApplication::exec();
}
