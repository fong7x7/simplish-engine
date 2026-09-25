#pragma once

/// @file logic-damage-cause.h
/// @brief What kind of thing hurt someone, in an event game logic hears.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// What a hurt, a death or a downing came from — the path the harm took,
/// where `LogicEvent::by` says whom to credit. `NONE` on every event that
/// is not one of those.
enum class LogicDamageCause : uint8_t {
  /// Not a hurt: a spawn, a removal, a revive.
  NONE,
  /// An actor's own attack: a bite, a charge, blowing itself up.
  ATTACK,
  /// A projectile landing.
  SHOT,
  /// Caught in an explosion.
  BLAST,
  /// Standing in a hazard pool as it bit.
  HAZARD,
  /// The game logic's own `damage`.
  LOGIC,
};

}  // namespace eng::game
