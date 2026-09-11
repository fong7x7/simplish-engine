#pragma once

/// @file enemy-spawn.h
/// @brief An enemy archetype, spawned somewhere.
/// @par Threading
/// Pure functions over value types.

#include <engine/math/vec3.h>
#include <game/actors/actor-spawn.h>
#include <game/content/enemy-definition.h>

namespace eng::game {

/// The actor @p enemy spawns as, its feet at @p at, facing @p yaw_degrees:
/// its behavior, side and body, as a prop given the same would have.
[[nodiscard]] ActorSpawn makeEnemySpawn(const EnemyDefinition& enemy, Vec3 at,
                                        float yaw_degrees);

}  // namespace eng::game
