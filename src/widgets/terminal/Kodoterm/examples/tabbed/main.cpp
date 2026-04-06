// SPDX-License-Identifier: MIT
// Author: Diego Iastrubni <diegoiast@gmail.com>

#include "TabbedTerminal.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    Q_INIT_RESOURCE(KodoTermThemes);
    QApplication app(argc, argv);
    app.setOrganizationName("KodoShell");
    app.setApplicationName("KodoShell");
    app.setWindowIcon(QIcon(":/KodoShell.svg"));
    TabbedTerminal mainWindow;
    mainWindow.setWindowTitle("KodoShell");
    mainWindow.show();
    return app.exec();
}