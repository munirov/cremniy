#include "viewmenu.h"
#include "ui/MenuBar/menufactory.h"
#include <QKeySequence>

static bool registered = [](){
    MenuFactory::instance().registerMenu("3", [](){
        return new ViewMenu();
    });
    return true;
}();

ViewMenu::ViewMenu() : BaseMenu("View") {
    m_wordWrap = new QAction("Word Wrap", this);
    m_wordWrap->setCheckable(true);
    m_wordWrap->setChecked(true);

    m_terminal = new QAction("Show terminal", this);
    m_terminal->setCheckable(true);
    m_terminal->setChecked(false);
    m_terminal->setShortcuts({
        QKeySequence("Ctrl+`"),
        QKeySequence("Ctrl+ё"),
    });

    m_fileTree = new QAction("File Tree", this);
    m_fileTree->setCheckable(true);
    m_fileTree->setChecked(true);
    m_fileTree->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));

    this->addAction(m_wordWrap);
    this->addSeparator();
    this->addAction(m_terminal);
    this->addAction(m_fileTree);
}

void ViewMenu::setupConnections(IDEWindow* ideWind){
    connect(m_terminal, &QAction::triggered, ideWind, &IDEWindow::on_Toggle_Terminal);
    connect(m_fileTree, &QAction::triggered, ideWind, &IDEWindow::on_Toggle_FileTree);
}
