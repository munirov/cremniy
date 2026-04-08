#include "editmenu.h"
#include "ui/MenuBar/menufactory.h"
#include <QKeySequence>

static bool registered = []() {
  MenuFactory::instance().registerMenu("2", []() { return new EditMenu(); });
  return true;
}();

EditMenu::EditMenu() : BaseMenu(tr("Edit")) {

  m_settings = new QAction(tr("Settings"), this);
  
    m_settings->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::Key_Comma),
        QKeySequence("Ctrl+б"),
    });
    
  this->addSeparator();
  this->addAction(m_settings);
}

void EditMenu::setupConnections(IDEWindow *ideWind) {
  connect(m_settings, &QAction::triggered, ideWind,
          &IDEWindow::on_openSettings);
}
