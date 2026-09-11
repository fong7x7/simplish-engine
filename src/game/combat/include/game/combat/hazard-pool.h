#pragma once

/// @file hazard-pool.h
/// @brief Every hazard pool on the floor, as structure-of-arrays.
/// @par Threading
/// Main-thread-only; mutated only inside a tick.

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/sim/entity-slots.h>
#include <game/content/faction.h>
#include <vector>

namespace eng::game {

/// Hazard pools a world holds at once; one lobbed past it is dropped.
inline constexpr uint32_t HAZARD_POOL_CAPACITY = 128;

/// The hazard pool (ADR-004): the puddles a spitter leaves. Every field is
/// simulation state and is hashed.
struct HazardPool {
  /// A pool with room for @p capacity hazards.
  explicit HazardPool(uint32_t capacity = HAZARD_POOL_CAPACITY);

  /// Handles, dense indices, and deferred destruction.
  sim::EntitySlots slots;
  /// Each hazard's centre, on the floor.
  std::vector<Vec2> position;
  /// Each one's radius, in tiles.
  std::vector<float> radius;
  /// Ticks each has left.
  std::vector<uint32_t> ticks_left;
  /// Ticks each has lasted so far: it bites on every
  /// `HAZARD_BITE_INTERVAL_TICKS`th.
  std::vector<uint32_t> age;
  /// Segments each takes from whoever it catches.
  std::vector<uint16_t> damage;
  /// The side that lobbed each: it hurts only the other.
  std::vector<Faction> side;
};

}  // namespace eng::game
