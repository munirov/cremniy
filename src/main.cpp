#include <QApplication>
#include <QCoreApplication>

#include "app/WelcomeWindow/welcomeform.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("cremniy");
    QCoreApplication::setApplicationName("Cremniy");
    a.setWindowIcon(QIcon(":/icons/icon.png"));

    QFile file(":/styles/style.qss");
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "Failed to open the style file: " << file.errorString();
        return 1;
    }
    QString styleSheet = QLatin1String(file.readAll());
    a.setStyleSheet(styleSheet);

    WelcomeForm wf;
    wf.show();
    return QCoreApplication::exec();
}