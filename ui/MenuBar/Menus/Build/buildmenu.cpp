#include "buildmenu.h"
#include "ui/MenuBar/menufactory.h"

static bool registered = [](){
    MenuFactory::instance().registerMenu("4", [](){
        return new BuildMenu();
    });
    return true;
}();

BuildMenu::BuildMenu() : BaseMenu("Build") {

}
