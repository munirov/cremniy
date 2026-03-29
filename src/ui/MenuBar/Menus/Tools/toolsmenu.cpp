#include "toolsmenu.h"
#include "dialogs/reversecalculatordialog.h"
#include "ui/MenuBar/menufactory.h"
#include <QKeySequence>

static bool registered = []() {
  MenuFactory::instance().registerMenu("5", []() { return new ToolsMenu(); });
  return true;
}();

ToolsMenu::ToolsMenu() : BaseMenu("Tools") {
  m_reverseCalculator = new QAction("Reverse Calculator", this);
  m_reverseCalculator->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
  this->addAction(m_reverseCalculator);
}

void ToolsMenu::setupConnections(IDEWindow *ideWind) {
  m_ideWindow = ideWind;
  connect(m_reverseCalculator, &QAction::triggered, this,
          &ToolsMenu::on_Open_ReverseCalculator);
}

void ToolsMenu::on_Open_ReverseCalculator() {
  auto *dlg = new ReverseCalculatorDialog(m_ideWindow);
  dlg->setAttribute(Qt::WA_DeleteOnClose, true);
  if (m_ideWindow) {
    dlg->adjustSize();
    dlg->move(m_ideWindow->geometry().center() - dlg->rect().center());
  }
  dlg->show();
  dlg->raise();
  dlg->activateWindow();
}
