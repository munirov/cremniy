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
    file.open(QFile::ReadOnly);
    QString styleSheet = QLatin1String(file.readAll());
    a.setStyleSheet(styleSheet);

    WelcomeForm wf;
    wf.show();
    return QCoreApplication::exec();
}