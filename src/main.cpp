#include <QApplication>
#include <QTranslator>
#include <QCoreApplication>

#include "app/WelcomeWindow/welcomeform.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("cremniy");

    QTranslator translator;
    if (translator.load(QLocale("ru"),"app", "_",QCoreApplication::applicationDirPath() + "/../Resources/translations/"))
        QApplication::installTranslator(&translator);

    QCoreApplication::setApplicationName("Cremniy");
    QApplication::setWindowIcon(QIcon(":/icons/icon.png"));

    QFile file(":/styles/style.qss");
    if (file.open(QFile::ReadOnly)) {
        const QString styleSheet = QLatin1String(file.readAll());
        a.setStyleSheet(styleSheet);
    }

    WelcomeForm wf;
    wf.show();
    return QCoreApplication::exec();
}