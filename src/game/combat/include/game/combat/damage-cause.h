#pragma once

/// @file damage-cause.h
/// @brief What kind of thing a hit came from.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// What dealt a `DamageEvent`: the path it reached its target by, as
/// against who is credited with it. A closed set, so whatever reports hits
/// can say what each one was.
enum class DamageCause : uint8_t {
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

/// How many causes there are.
inline constexpr uint8_t DAMAGE_CAUSE_COUNT = 5;

}  // namespace eng::game
