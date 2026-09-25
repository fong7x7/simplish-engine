#include <engine/math/sin-cos.h>
#include <game/sdk/ring-spawn.h>

namespace eng::game::sdk {

namespace {

  /// Spawn one of @p ring's actors at @p at, facing @p yaw. Whether it
  /// was queued.
  bool spawnOne(GameLogicWorld& world, const RingSpawn& ring, Vec3 at,
                float yaw) {
    if (ring.placement == RingPlacement::WALKABLE_ONLY && !world.walkable(at)) {
      return false;
    }
    if (!ring.enemy.empty()) {
      return world.spawnEnemy(ring.enemy, at, ring.actor.id);
    }
    LogicSpawn spawn = ring.actor;
    spawn.at = at;
    spawn.yaw_degrees = yaw;
    return world.spawnActor(spawn);
  }

}  // namespace

uint32_t spawnRing(GameLogicWorld& world, const RingSpawn& ring) {
  uint32_t spawned = 0;
  const float step =
      ring.count == 0 ? 0.0F : 360.0F / static_cast<float>(ring.count);
  for (uint32_t k = 0; k < ring.count; ++k) {
    const float degrees = ring.start_degrees + step * static_cast<float>(k);
    const math::SinCos turn = math::sinCosDegrees(degrees);
    const Vec3 at{ring.centre.x + turn.cos * ring.radius,
                  ring.centre.y + turn.sin * ring.radius, ring.centre.z};
    // Facing the centre: the way round the circle, turned about.
    spawned += spawnOne(world, ring, at, degrees + 180.0F) ? 1 : 0;
  }
  return spawned;
}

}  // namespace eng::game::sdk
