#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QImageReader>
#include <QDirIterator>
#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QResource>
#include <QFontDatabase>
#include <QUrl>

#include "app/WelcomeWindow/WelcomeForm/welcome_form.h"
#include "core/locale/LanguageManager.h"
#include "core/update/updatechecker.h"
#include "libs/CodeEditor/include/languages/LanguageRegistration.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Force-links and registers every supported syntax-highlighting
    // language. Must run before any CustomCodeEditor is constructed. See
    // LanguageRegistration.h for why this explicit call is necessary.
    registerAllLanguages();

    QCoreApplication::setOrganizationName("Munirov");
    QCoreApplication::setApplicationName("Cremniy");

#if !defined(Q_OS_MAC)
    a.setWindowIcon(QIcon(":/icons/icon.svg"));
#endif

    // - - Fonts - -
    LanguageManager::instance().loadUserDefaultLocale();

    int jbFontRegId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");
    int jbFontBoldId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Bold.ttf");
    int jbFontItalId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Italic.ttf");

    QStringList fontFamilies = QFontDatabase::applicationFontFamilies(jbFontRegId);
    QString jbFontFamily = !fontFamilies.isEmpty() ? fontFamilies.at(0) : "Sans Serif";

    qDebug() << jbFontFamily;

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
    QApplication::setStyle("Fusion");

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

    //cli path selector
    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        const QString projectPath = args.at(1);
        if (QDir(projectPath).exists()) {
            wf.openProject(projectPath);
        } else {
            qWarning() << "Project path does not exist:" << projectPath;
            wf.show();
        }
    } else {
        wf.show();
    }

    /* Update check */
    auto* updateChecker = new core::UpdateChecker(&wf);

    QObject::connect(
        updateChecker,
        &core::UpdateChecker::updateAvailable,
        &wf,
        [&wf](const QString& latestVersion) {
            QMessageBox updateDialog(
                QMessageBox::Information,
                QObject::tr("Update available"),
                QObject::tr("A new version of Cremniy is available: %1.").arg(latestVersion),
                QMessageBox::NoButton,
                &wf
            );

            auto* openRelease = updateDialog.addButton(
                QObject::tr("Open release page"),
                QMessageBox::AcceptRole
            );
            auto* later = updateDialog.addButton(
                QObject::tr("Later"),
                QMessageBox::RejectRole
            );
            Q_UNUSED(later);

            updateDialog.exec();

            if (updateDialog.clickedButton() == openRelease) {
                QDesktopServices::openUrl(
                    QUrl(QStringLiteral("https://github.com/munirov/cremniy/releases/latest"))
                );
            }
        }
    );

    QObject::connect(
        updateChecker,
        &core::UpdateChecker::checkFailed,
        &wf,
        [](const QString& reason) {
            qWarning() << "Update check failed:" << reason;
        }
    );

    updateChecker->checkForUpdate();

    return QCoreApplication::exec();
}
