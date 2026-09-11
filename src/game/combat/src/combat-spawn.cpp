#include <game/combat/combat-system.h>

namespace eng::game {

namespace {

  /// Spawn one projectile as @p shot asks, if the pool has room.
  void spawnShot(ProjectilePool& projectiles, const ShotRequest& shot) {
    if (!projectiles.slots.spawn()) {
      return;
    }
    // `spawn` always places a new entity at the end of the dense range.
    const uint32_t i = projectiles.slots.size() - 1U;
    projectiles.position[i] = shot.from;
    projectiles.velocity[i] = shot.velocity;
    projectiles.ticks_left[i] = PROJECTILE_FLIGHT_TICKS;
    projectiles.damage[i] = shot.damage;
    projectiles.side[i] = shot.side;
  }

  /// Spawn one hazard as @p request asks, if the pool has room.
  void spawnHazard(HazardPool& hazards, const HazardRequest& request) {
    if (request.ticks == 0 || !hazards.slots.spawn()) {
      return;
    }
    const uint32_t i = hazards.slots.size() - 1U;
    hazards.position[i] = request.at;
    hazards.radius[i] = request.radius;
    hazards.ticks_left[i] = request.ticks;
    hazards.age[i] = 0;
    hazards.damage[i] = request.damage;
    hazards.side[i] = request.side;
  }

}  // namespace

void spawnCombatEffects(ProjectilePool& projectiles, HazardPool& hazards,
                        const CombatEffects& effects) {
  for (const ShotRequest& shot : effects.shots) {
    spawnShot(projectiles, shot);
  }
  for (const HazardRequest& request : effects.hazards) {
    spawnHazard(hazards, request);
  }
}

}  // namespace eng::game
