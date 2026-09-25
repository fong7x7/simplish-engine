#pragma once

/// @file deployed-hashes.h
/// @brief Which tick hashes a run of a deployed game keeps.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// Whether a run keeps every tick's hash, to be compared with another
/// run's, or only the last.
/// @thread_safety Immutable value type.
enum class DeployedHashes : uint8_t {
  /// Only the last tick's, in `DeployedGameRun::hash`.
  LAST_ONLY,
  /// Every tick's too, in `DeployedGameRun::tick_hashes`.
  EVERY_TICK,
};

}  // namespace eng::editor
