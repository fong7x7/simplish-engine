#pragma once

/// @file actor-route.h
/// @brief A route an actor patrols: waypoints in walking order.
/// @par Threading
/// A value type; read-only once a run starts.

#include <cstdint>
#include <engine/math/vec2.h>
#include <vector>

namespace eng::game {

/// What `ActorPool::route` holds for an actor with no route.
inline constexpr uint16_t ACTOR_NO_ROUTE = 0xFFFF;

/// The waypoints a patrol walks, in order. Part of the run's setup, fixed
/// for the run, and held by the world — an actor names one by index.
struct ActorRoute {
  /// The waypoints, in walking order, on the floor.
  std::vector<Vec2> points{};
};

}  // namespace eng::game
