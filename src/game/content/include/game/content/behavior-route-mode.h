#pragma once

/// @file behavior-route-mode.h
/// @brief How a patrol walks its route once it reaches the end.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// What a patrol does at the last waypoint of its route.
enum class BehaviorRouteMode : uint8_t {
  /// Go on to the first again: a round.
  LOOP,
  /// Turn and walk it back the other way: a beat.
  PING_PONG,
};

}  // namespace eng::game
