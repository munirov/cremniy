#include "viewmenu.h"
#include "ui/MenuBar/menufactory.h"
#include <QKeySequence>

#include <QActionGroup>

static bool registered = [](){
    MenuFactory::instance().registerMenu("3", [](){
        return new ViewMenu();
    });
    return true;
}();

ViewMenu::ViewMenu() : BaseMenu(tr("View")) {
    m_wordWrap = new QAction(tr("Word Wrap"), this);
    m_wordWrap->setCheckable(true);
    m_wordWrap->setChecked(true);

    m_tabReplace = new QAction(tr("Insert spaces instead of tab"), this);
    m_tabReplace->setCheckable(true);
    m_tabReplace->setChecked(false);

    auto* tabWidthMenu = new QMenu(tr("Tab Width"), this);
    QActionGroup* tabWidthGroup = new QActionGroup(this);
    tabWidthGroup->setExclusive(true);
    m_tabWidth2 = tabWidthMenu->addAction("2");
    m_tabWidth4 = tabWidthMenu->addAction("4");
    m_tabWidth8 = tabWidthMenu->addAction("8");
    for (QAction* action : {m_tabWidth2, m_tabWidth4, m_tabWidth8}) {
        action->setCheckable(true);
        tabWidthGroup->addAction(action);
    }
    m_tabWidth4->setChecked(true);

    m_terminal = new QAction(tr("Show terminal"), this);
    m_terminal->setCheckable(true);
    m_terminal->setChecked(false);
    m_terminal->setShortcuts({
        QKeySequence("Ctrl+`"),
        QKeySequence("Ctrl+ё"),
    });

    m_fileTree = new QAction(tr("File Tree"), this);
    m_fileTree->setCheckable(true);
    m_fileTree->setChecked(true);
    m_fileTree->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));

    this->addAction(m_wordWrap);
    this->addAction(m_tabReplace);
    this->addMenu(tabWidthMenu);
    this->addSeparator();
    this->addAction(m_terminal);
    this->addAction(m_fileTree);
}

void ViewMenu::setupConnections(IDEWindow* ideWind){
    connect(m_wordWrap, &QAction::triggered, ideWind, &IDEWindow::on_SetWordWrap);
    connect(m_tabReplace, &QAction::triggered, ideWind, &IDEWindow::on_SetTabReplace);
    connect(m_tabWidth2, &QAction::triggered, ideWind, [ideWind]() { ideWind->on_SetTabWidth(2); });
    connect(m_tabWidth4, &QAction::triggered, ideWind, [ideWind]() { ideWind->on_SetTabWidth(4); });
    connect(m_tabWidth8, &QAction::triggered, ideWind, [ideWind]() { ideWind->on_SetTabWidth(8); });
    connect(m_terminal, &QAction::triggered, ideWind, &IDEWindow::on_Toggle_Terminal);
    connect(m_fileTree, &QAction::triggered, ideWind, &IDEWindow::on_Toggle_FileTree);
}
