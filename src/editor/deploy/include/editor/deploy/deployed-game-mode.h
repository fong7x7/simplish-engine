#pragma once

/// @file deployed-game-mode.h
/// @brief Whether a deployed game plays alone, or which end of a session.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// What `simplish-game` is asked to be (ADR-013).
/// @thread_safety Immutable value type.
enum class DeployedGameMode : uint8_t {
  SOLO,    ///< One process, every player a stand-in; no network
  SERVE,   ///< A dedicated server: no local player, simulating as reference
  HOST,    ///< A listen server with a local player on it
  JOIN,    ///< A client of someone else's server
  VERIFY,  ///< Play a recorded replay back and check it reproduces
  FIND,    ///< List the sessions on the local network
};

}  // namespace eng::editor
