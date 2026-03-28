#include "filemenu.h"
#include "ui/MenuBar/menufactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("1", [](){
        return new FileMenu();
    });
    return true;
}();

FileMenu::FileMenu() : BaseMenu("File") {

    m_openProject = new QAction("New Project", this);
    m_newProject = new QAction("Open Project", this);
    m_saveFile = new QAction("Save File", this);
    m_closeProject = new QAction("Close Project", this);

    m_saveFile->setShortcut(QKeySequence::Save);

    this->addAction(m_openProject);
    this->addAction(m_newProject);
    this->addSeparator();
    this->addAction(m_saveFile);
    this->addSeparator();
    this->addAction(m_closeProject);

}

void FileMenu::setupConnections(IDEWindow* ideWind){
    connect(m_newProject, &QAction::triggered, ideWind, &IDEWindow::on_NewProject);
    connect(m_openProject, &QAction::triggered, ideWind, &IDEWindow::on_OpenProject);
    connect(m_saveFile, &QAction::triggered, ideWind, &IDEWindow::on_SaveFile);
    connect(m_closeProject, &QAction::triggered, ideWind, &IDEWindow::on_ClosingProject);
}