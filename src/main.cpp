#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QImageReader>
#include <QDirIterator>
#include <QDebug>
#include <QResource>
#include <QFontDatabase>

#include "app/WelcomeWindow/welcomeform.h"
#include "core/locale/LanguageManager.h"

#ifdef Q_OS_WIN
#include <windows.h>

namespace {

using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);

void applyDarkTitleBar(QWidget* widget)
{
    if (!widget || !widget->isWindow())
        return;

    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd)
        return;

    static DwmSetWindowAttributeFn setAttribute = nullptr;
    if (!setAttribute) {
        const HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        if (!dwm)
            return;
        setAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
            GetProcAddress(dwm, "DwmSetWindowAttribute"));
        if (!setAttribute)
            return;
    }

    const BOOL dark = TRUE;
    /* DWMWA_USE_IMMERSIVE_DARK_MODE is attribute 20 on Windows 10 2004+ and
       attribute 19 on earlier builds; applying both is harmless because only
       the one supported by the running system is accepted */
    setAttribute(hwnd, 19, &dark, sizeof(dark));
    setAttribute(hwnd, 20, &dark, sizeof(dark));
}

class DarkTitleBarFilter : public QObject
{
public:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
            if (auto* widget = qobject_cast<QWidget*>(watched))
                applyDarkTitleBar(widget);
        }
        return false;
    }
};

} // namespace
#endif

int main(int argc, char *argv[])
{
    #ifdef Q_OS_LINUX
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    #endif
    QApplication a(argc, argv);

    #ifdef Q_OS_WIN
    /* Dark native title bar for all top-level windows */
    DarkTitleBarFilter darkTitleBarFilter;
    a.installEventFilter(&darkTitleBarFilter);
    #endif

    QCoreApplication::setOrganizationName("Munirov");
    QCoreApplication::setApplicationName("Cremniy");
    a.setWindowIcon(QIcon(":/icons/icon.svg"));

    // - - Fonts - -
    LanguageManager::instance().loadUserDefaultLocale();

    int jbFontRegId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");
    int jbFontBoldId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Bold.ttf");
    int jbFontItalId = QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Italic.ttf");

    const QStringList jbFontFamilies = QFontDatabase::applicationFontFamilies(jbFontRegId);
    QString jbFontFamily;
    if (!jbFontFamilies.isEmpty())
        jbFontFamily = jbFontFamilies.at(0);

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
    wf.show();
    return QCoreApplication::exec();
}
