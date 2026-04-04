#include "editmenu.h"
#include "ui/MenuBar/menufactory.h"
#include <QAction>
#include <QKeySequence>

static bool registered = []() {
  MenuFactory::instance().registerMenu("2", []() { return new EditMenu(); });
  return true;
}();

EditMenu::EditMenu() : BaseMenu("Edit") {

  m_findInProject = new QAction(tr("Find in Project"), this);
  m_findInProject->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
  m_findInProject->setShortcutContext(Qt::ApplicationShortcut);

  m_settings = new QAction("Settings", this);
  
    m_settings->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::Key_Comma),
        QKeySequence("Ctrl+б"),
    });
    
  this->addAction(m_findInProject);
  this->addSeparator();
  this->addAction(m_settings);
}

void EditMenu::setupConnections(IDEWindow *ideWind) {
  ideWind->addAction(m_findInProject);
  connect(m_findInProject, &QAction::triggered, ideWind,
          &IDEWindow::showProjectSearch);
  connect(m_settings, &QAction::triggered, ideWind,
          &IDEWindow::on_openSettings);
}
