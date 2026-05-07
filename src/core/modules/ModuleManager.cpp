#include "core/modules/ModuleManager.h"


ModuleManager& ModuleManager::instance() {
    static ModuleManager inst;
    return inst;
}