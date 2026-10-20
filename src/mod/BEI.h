#pragma once

#include "ll/api/mod/NativeMod.h"

namespace bei {

/// Mod entry point. Wires all modules together; modules never call each other's install().
class BEI {
public:
    static BEI& getInstance();

    BEI() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace bei
