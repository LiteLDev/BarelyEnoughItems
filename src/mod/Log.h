#pragma once

#include "ll/api/io/Logger.h"

namespace bei {

/// The mod's logger (writes to the LeviLamina console/log as "BEI").
[[nodiscard]] ll::io::Logger& log();

} // namespace bei
