#pragma once

/// @file deployed-pace.h
/// @brief How fast a networked deployed game samples its player's input.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>

namespace eng::editor {

/// What paces a networked run's input. The simulation never sees this —
/// it steps on frames — but a session runs no faster than its slowest
/// player's input.
/// @thread_safety Immutable value type.
enum class DeployedPace : uint8_t {
  REAL_TIME,  ///< Sixty inputs a second of real time, as a player's would be
  FAST,       ///< An input whenever the session will take one: tests and CI
};

}  // namespace eng::editor
