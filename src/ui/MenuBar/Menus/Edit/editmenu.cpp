#include "editmenu.h"
#include "ui/MenuBar/menufactory.h"
#include <QKeySequence>

static bool registered = []() {
  MenuFactory::instance().registerMenu("2", []() { return new EditMenu(); });
  return true;
}();

EditMenu::EditMenu() : BaseMenu("Edit") {

  m_settings = new QAction("Settings", this);
  m_settings->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
  this->addSeparator();
  this->addAction(m_settings);
}

void EditMenu::setupConnections(IDEWindow *ideWind) {
  connect(m_settings, &QAction::triggered, ideWind,
          &IDEWindow::on_openSettings);
}
