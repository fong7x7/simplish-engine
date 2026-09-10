#pragma once

/// @file player-pool.h
/// @brief Every player in the simulation, as structure-of-arrays.
/// @par Threading
/// Main-thread-only; mutated only inside a tick.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <engine/sim/entity-slots.h>
#include <engine/sim/tick-input.h>
#include <vector>

namespace eng::game {

/// Players a session holds at once: one per input slot.
inline constexpr uint32_t PLAYER_POOL_CAPACITY =
    static_cast<uint32_t>(sim::MAX_PLAYERS);

/// The player pool (ADR-004): handle bookkeeping plus one array per field,
/// each indexed by dense index. A system reads the fields it needs and
/// nothing else.
struct PlayerPool {
  /// Handles, dense indices, and deferred destruction.
  sim::EntitySlots slots{PLAYER_POOL_CAPACITY};
  /// Where each player's feet are, in tiles.
  std::vector<Vec3> position = std::vector<Vec3>(PLAYER_POOL_CAPACITY);
  /// The unit direction each player is aiming in, world X and Y. Kept
  /// while no aim is given, so letting go of the stick does not snap it.
  std::vector<Vec2> aim = std::vector<Vec2>(PLAYER_POOL_CAPACITY);
  /// Which `TickInput::players` entry drives each player, 0 to 3.
  std::vector<uint8_t> input_slot = std::vector<uint8_t>(PLAYER_POOL_CAPACITY);
};

}  // namespace eng::game
