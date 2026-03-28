#ifndef FILEMENU_H
#define FILEMENU_H

#include "ui/MenuBar/basemenu.h"

class FileMenu : public BaseMenu
{
    Q_OBJECT
private:
    QAction* m_openProject;
    QAction* m_newProject;
    QAction* m_saveFile;
    QAction* m_closeProject;
public:
    FileMenu();
    void setupConnections(IDEWindow* ideWind);
};

#endif // FILEMENU_H
