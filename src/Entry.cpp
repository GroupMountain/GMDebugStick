#include "Entry.h"
#include <gmlib/gm/i18n/ResourceI18n.h>
#include <ll/api/mod/RegisterHelper.h>

namespace DebugStick {

Entry& Entry::getInstance() {
    static Entry instance;
    return instance;
}

bool Entry::load() {
    auto version  = getSelf().getManifest().version.value_or(ll::data::Version{0, 0, 0});
    auto resource = gmlib::i18n::ResourceI18n(
        getSelf().getModDir(),
        getSelf().getName(),
        version.major,
        version.minor,
        version.patch
    );
    resource.addLanguagesFromDirectory(getSelf().getLangDir());
    resource.loadAllLanguages();
    return true;
}

bool Entry::enable() { return true; }

bool Entry::disable() { return true; }

bool Entry::unload() { return true; }

} // namespace DebugStick

LL_REGISTER_MOD(DebugStick::Entry, DebugStick::Entry::getInstance());