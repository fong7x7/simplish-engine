#pragma once

/// @file behavior-senses.h
/// @brief How far and how widely an actor perceives, and how long it
/// remembers.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/content/behavior-targets.h>

namespace eng::game {

/// An actor's senses.
struct BehaviorSenses {
  /// How far it can see, in tiles.
  float sight_range = 10.0F;
  /// How wide its view is, in degrees, centred on its facing: 360 sees all
  /// round.
  float view_degrees = 180.0F;
  /// How far away it hears a player fire, in tiles.
  float hearing_range = 6.0F;
  /// How long it remembers a target it has stopped perceiving, in ticks,
  /// before it forgets them.
  uint32_t memory_ticks = 300;
  /// Whom it takes as a target.
  BehaviorTargets targets = BehaviorTargets::PLAYERS;
};

}  // namespace eng::game
