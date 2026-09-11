#include <game/actors/enemy-spawn.h>

namespace eng::game {

static_assert(ENEMY_DEFAULT_RADIUS_TILES == ACTOR_DEFAULT_RADIUS_TILES &&
                  ENEMY_DEFAULT_HEIGHT_TILES == ACTOR_DEFAULT_HEIGHT_TILES,
              "an archetype that says nothing of its body is an actor's size");

ActorSpawn makeEnemySpawn(const EnemyDefinition& enemy, Vec3 at,
                          float yaw_degrees) {
  return {.at = at,
          .yaw_degrees = yaw_degrees,
          .behavior = enemy.behavior,
          .faction = enemy.faction,
          .radius = enemy.radius,
          .height = enemy.height};
}

}  // namespace eng::game
