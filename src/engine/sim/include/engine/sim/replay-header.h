#pragma once

/// @file replay-header.h
/// @brief What a replay must start from to reproduce its run.
/// @par Threading
/// A value type.

#include <array>
#include <cstdint>
#include <engine/sim/tick-input.h>
#include <string>

namespace eng::sim {

/// The initial conditions of a recorded run. With the input stream, these
/// are all a replay is (Engine REQUIREMENTS §4.4): whoever plays it back
/// builds the simulation from them, then feeds it the inputs.
struct ReplayHeader {
  /// The level the run was played in.
  std::string level_id;
  /// Hash of the content the run simulated against. A replay played against
  /// different content will not reproduce, and this is how that is noticed
  /// before the first tick rather than at a divergence.
  uint64_t content_hash = 0;
  /// The session seed every RNG stream was derived from.
  uint64_t seed = 0;
  /// Players in the session, 1 to `MAX_PLAYERS`.
  uint8_t player_count = 1;
  /// What each player played as, by input slot: an id the game resolves
  /// against its content, or empty for its default. Opaque to the engine;
  /// it is here because a run's players are part of what it started from,
  /// and a replay that forgot them would replay a different run. Entries at
  /// or beyond `player_count` are empty.
  std::array<std::string, MAX_PLAYERS> characters{};

  /// Headers are equal when every field is.
  bool operator==(const ReplayHeader&) const = default;
};

}  // namespace eng::sim
