#include "mod/MyMod.h"
#include "mod/Config.h"

#include <memory>

#include "ll/api/mod/RegisterHelper.h"


Config config;


void ModInit();
void RegReloadCommand();
void RegListener();

namespace my_mod {

    static std::unique_ptr<MyMod> instance;

    MyMod& MyMod::getInstance() { return *instance; }

    bool MyMod::load() {
        getSelf().getLogger().debug("Loading...");
        // Code for loading the mod goes here.
        ModInit();
        return true;
    }

    bool MyMod::enable() {
        getSelf().getLogger().debug("Enabling...");
        // Code for enabling the mod goes here.
        RegReloadCommand();
        RegListener();
        return true;
    }

    bool MyMod::disable() {
        getSelf().getLogger().debug("Disabling...");
        // Code for disabling the mod goes here.
        return true;
    }

} // namespace my_mod

LL_REGISTER_MOD(my_mod::MyMod, my_mod::instance);
