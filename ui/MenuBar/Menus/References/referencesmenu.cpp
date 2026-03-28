#include "referencesmenu.h"
#include "ui/MenuBar/menufactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("6", [](){
        return new ReferencesMenu();
    });
    return true;
}();

ReferencesMenu::ReferencesMenu() : BaseMenu("References") {
    m_asciiChars = new QAction("ASCII characters", this);
    m_keybScancodes = new QAction("Keyboard Scancodes", this);

    this->addAction(m_asciiChars);
    this->addAction(m_keybScancodes);
}
