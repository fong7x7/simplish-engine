#pragma once

/// @file lockstep-server-config.h
/// @brief How a lockstep server runs its session.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/sim/tick-input.h>

namespace eng::net {

/// Input delay when not told otherwise: three ticks, 50 ms (ADR-005).
inline constexpr uint8_t NET_DEFAULT_INPUT_DELAY = 3;

/// Whether a server keeps the frames it sends, for `takeFrame`.
enum class NetServerFrames : uint8_t {
  RELAY,  ///< Sent and forgotten: a server whose host plays as a client
  KEEP,   ///< Also kept: a dedicated server that simulates the run itself
};

/// Everything a `LockstepServer` is configured with.
struct LockstepServerConfig {
  /// The content hash every client's `NetHello` must carry.
  uint64_t content_hash = 0;
  /// Seats, 1 to `sim::MAX_PLAYERS`.
  uint8_t seats = sim::MAX_PLAYERS;
  /// Ticks between a client sampling its input and the tick it is for.
  /// At least 1; tick 0 to `input_delay - 1` run on no input.
  uint8_t input_delay = NET_DEFAULT_INPUT_DELAY;
  /// Whether sent frames are kept for `takeFrame`.
  NetServerFrames frames = NetServerFrames::RELAY;
};

}  // namespace eng::net
