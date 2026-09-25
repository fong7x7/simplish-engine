#pragma once

/// @file logic-player.h
/// @brief One player, as game logic reads it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <game/logic/logic-player-status.h>
#include <game/logic/logic-target.h>

namespace eng::game {

/// A copy of one player's state at the moment it was read. Changing it
/// changes nothing: game logic writes through `GameLogicWorld`.
struct LogicPlayer {
  /// Who this is, for `GameLogicWorld::damage` and `heal`.
  LogicTarget target{};
  /// Which input slot drives them, 0 to 3: player 1 is slot 0.
  uint8_t slot = 0;
  /// Where their feet are, in tiles.
  Vec3 position{};
  /// The unit direction they aim in, world X and Y.
  Vec2 aim{};
  /// Health segments left.
  uint16_t health = 0;
  /// Health segments a full bar holds.
  uint16_t max_health = 0;
  /// Up, down or out.
  LogicPlayerStatus status = LogicPlayerStatus::UP;
};

}  // namespace eng::game
