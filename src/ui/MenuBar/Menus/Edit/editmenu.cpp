#include "editmenu.h"
#include "ui/MenuBar/menufactory.h"
#include <QKeySequence>

static bool registered = []() {
  MenuFactory::instance().registerMenu("2", []() { return new EditMenu(); });
  return true;
}();

EditMenu::EditMenu() : BaseMenu(tr("Edit")) {

  m_find = new QAction(tr("Find"), this);
  m_find->setShortcut(QKeySequence::Find);

  m_findInProject = new QAction(tr("Find in Project"), this);
  m_findInProject->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));

  m_settings = new QAction(tr("Settings"), this);
  m_settings->setShortcuts({
      QKeySequence(Qt::CTRL | Qt::Key_Comma),
      QKeySequence("Ctrl+б"),
  });

  this->addAction(m_find);
  this->addAction(m_findInProject);
  this->addSeparator();
  this->addAction(m_settings);
}

void EditMenu::setupConnections(IDEWindow *ideWind) {
  connect(m_find, &QAction::triggered, ideWind,
          &IDEWindow::on_FindInFile);
  connect(m_findInProject, &QAction::triggered, ideWind,
          &IDEWindow::on_FindInProject);
  connect(m_settings, &QAction::triggered, ideWind,
          &IDEWindow::on_openSettings);
}
