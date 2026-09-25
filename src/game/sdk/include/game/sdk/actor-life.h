#pragma once

/// @file actor-life.h
/// @brief Whether a query counts actors dying this tick.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game::sdk {

/// Which actors a query takes, by whether they are alive.
enum class ActorLife : uint8_t {
  /// Only those with health left.
  ALIVE,
  /// Those dying this tick too.
  ANY,
};

}  // namespace eng::game::sdk
