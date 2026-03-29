#include <QApplication>
#include <QFile>
#include <QIcon>
#include "app/WelcomeWindow/welcomeform.h"
#include "utils/appsettings.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("cremniy");
    QCoreApplication::setApplicationName("Cremniy");

    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(":/icons/icon.png"));

    QString qssPath = AppSettings::themeQssPath(); 
    
    QFile file(qssPath);
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QString::fromUtf8(file.readAll());
        a.setStyleSheet(styleSheet);
        file.close();
    } else {
        QFile defaultFile(":/styles/dark.qss");
        if (defaultFile.open(QFile::ReadOnly)) {
            a.setStyleSheet(QString::fromUtf8(defaultFile.readAll()));
        }
    }

    WelcomeForm wf;
    wf.show();
    
    return a.exec();
}