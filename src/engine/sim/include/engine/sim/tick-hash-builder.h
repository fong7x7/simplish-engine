#pragma once

/// @file tick-hash-builder.h
/// @brief Collects a tick hash one subsystem section at a time.
/// @par Threading
/// A value type; one instance per tick being hashed.

#include <array>
#include <cstddef>
#include <cstdint>
#include <engine/sim/state-hasher.h>
#include <engine/sim/tick-hash.h>
#include <string_view>

namespace eng::sim {

/// What `SimulationSystems::hashState` writes into. Each subsystem starts a
/// section and folds its state into the hasher the section returns:
///
/// @code
///   StateHasher& enemies = builder.section("enemies");
///   enemies_.slots.hashInto(enemies);
///   enemies.addSpan(std::span<const Vec2>(enemies_.position));
/// @endcode
///
/// Section order is the combination order, so it is part of the hash. Keep
/// it fixed — the natural choice is the order systems run in.
class TickHashBuilder {
public:
  /// Starts a section named `name` — a string literal — and returns its
  /// hasher. At most `MAX_TICK_HASH_SECTIONS` sections.
  StateHasher& section(std::string_view name);

  /// The tick hash for `tick`, combining every section started so far.
  [[nodiscard]] TickHash finish(uint64_t tick) const;

private:
  /// One hasher per section, in the order the sections were started.
  std::array<StateHasher, MAX_TICK_HASH_SECTIONS> hashers_{};
  /// Each section's name, parallel to `hashers_`.
  std::array<std::string_view, MAX_TICK_HASH_SECTIONS> names_{};
  /// Sections started.
  std::size_t count_ = 0;
};

}  // namespace eng::sim
