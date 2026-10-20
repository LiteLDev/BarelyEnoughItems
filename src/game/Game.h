#pragma once

class ClientInstance;
class ClientLevel;
class LocalPlayer;

namespace bei::game {

/// Current client instance, or nullptr before the game finished starting.
[[nodiscard]] ClientInstance* currentClient();
/// Current client level, or nullptr when not in a world.
[[nodiscard]] ClientLevel* currentLevel();
/// Local player, or nullptr when not in a world.
[[nodiscard]] LocalPlayer* currentPlayer();

} // namespace bei::game
