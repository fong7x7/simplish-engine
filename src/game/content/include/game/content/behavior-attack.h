#pragma once

/// @file behavior-attack.h
/// @brief What a state's attack does: how hard, how often, how far.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The numbers an attacking state strikes with. Each attack action reads
/// the ones it needs — `melee` and `charge` the damage, reach and cooldown;
/// `fire` the volley's count, spread and speed; `spit` the pool's radius and
/// how long it lasts; `detonate` the blast's radius — and ignores the rest.
/// `defaultBehaviorState` fills in each action's own defaults.
struct BehaviorAttack {
  /// Health segments each hit takes. 0 attacks with nothing.
  uint16_t damage = 0;
  /// Ticks between one attack and the next.
  uint32_t cooldown_ticks = 0;
  /// How far past touching a melee or a charge reaches, in tiles.
  float reach_tiles = 0.0F;
  /// Projectiles in one volley.
  uint8_t count = 1;
  /// How wide a volley fans, in degrees, centred on the target.
  float spread_degrees = 0.0F;
  /// How fast a projectile flies, in tiles a second.
  float speed = 0.0F;
  /// A pool's or a blast's radius, in tiles.
  float radius = 0.0F;
  /// How long a pool lasts, in ticks.
  uint32_t duration_ticks = 0;
};

}  // namespace eng::game
