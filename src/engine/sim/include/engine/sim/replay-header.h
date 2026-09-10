#pragma once

/// @file replay-header.h
/// @brief What a replay must start from to reproduce its run.
/// @par Threading
/// A value type.

#include <cstdint>
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

  /// Headers are equal when every field is.
  bool operator==(const ReplayHeader&) const = default;
};

}  // namespace eng::sim
