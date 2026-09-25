#include <game/combat/combat-system.h>

namespace eng::game {

namespace {

  /// Spawn one projectile as @p shot asks, if the pool has room. Whether
  /// it had.
  bool spawnShot(ProjectilePool& projectiles, const ShotRequest& shot) {
    if (!projectiles.slots.spawn()) {
      return false;
    }
    // `spawn` always places a new entity at the end of the dense range.
    const uint32_t i = projectiles.slots.size() - 1U;
    projectiles.position[i] = shot.from;
    projectiles.velocity[i] = shot.velocity;
    projectiles.ticks_left[i] = PROJECTILE_FLIGHT_TICKS;
    projectiles.damage[i] = shot.damage;
    projectiles.side[i] = shot.side;
    projectiles.source[i] = shot.source;
    return true;
  }

  /// The cue for @p shot leaving whoever fired it.
  CombatCue firedCue(const ShotRequest& shot) {
    return {CombatCueKind::SHOT_FIRED,
            {shot.from.x, shot.from.y, PROJECTILE_Z_TILES},
            shot.velocity,
            0.0F,
            shot.side};
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
    hazards.source[i] = request.source;
  }

}  // namespace

void spawnCombatEffects(ProjectilePool& projectiles, HazardPool& hazards,
                        const CombatEffects& effects,
                        std::vector<CombatCue>& cues) {
  for (const ShotRequest& shot : effects.shots) {
    if (spawnShot(projectiles, shot)) {
      cueCombat(cues, firedCue(shot));
    }
  }
  for (const HazardRequest& request : effects.hazards) {
    spawnHazard(hazards, request);
  }
}

}  // namespace eng::game
