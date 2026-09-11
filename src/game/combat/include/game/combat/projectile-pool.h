#pragma once

/// @file projectile-pool.h
/// @brief Every projectile in flight, as structure-of-arrays.
/// @par Threading
/// Main-thread-only; mutated only inside a tick.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/sim/entity-slots.h>
#include <game/content/faction.h>
#include <vector>

namespace eng::game {

/// Projectiles a world holds at once; a shot fired past it is dropped.
inline constexpr uint32_t PROJECTILE_POOL_CAPACITY = 1024;

/// The projectile pool (ADR-004): what actors fire, until it hits someone,
/// hits something, or runs out of flight. Every field is simulation state
/// and is hashed.
struct ProjectilePool {
  /// A pool with room for @p capacity projectiles.
  explicit ProjectilePool(uint32_t capacity = PROJECTILE_POOL_CAPACITY);

  /// Handles, dense indices, and deferred destruction.
  sim::EntitySlots slots;
  /// Where each projectile is, on the floor, in tiles.
  std::vector<Vec2> position;
  /// How far each moves a tick, and which way.
  std::vector<Vec2> velocity;
  /// Ticks of flight each has left.
  std::vector<uint32_t> ticks_left;
  /// Segments whoever each hits loses.
  std::vector<uint16_t> damage;
  /// The side that fired each: it hurts only the other.
  std::vector<Faction> side;
};

}  // namespace eng::game
