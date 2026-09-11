#pragma once

/// @file combat-system.h
/// @brief What the tick does to projectiles, hazards and blasts.
/// @par Threading
/// Main-thread-only; called from the tick's phases.

#include <cstdint>
#include <engine/sim/state-hasher.h>
#include <game/combat/combat-scene.h>
#include <game/combat/hazard-pool.h>
#include <game/combat/projectile-pool.h>

namespace eng::game {

/// Radius of every projectile, in tiles.
inline constexpr float PROJECTILE_RADIUS_TILES = 0.12F;

/// Height a projectile flies at, in tiles: chest height, so a crate stops
/// it and a rug does not.
inline constexpr float PROJECTILE_Z_TILES = 0.9F;

/// How long a projectile flies before it falls, in ticks: three seconds.
inline constexpr uint32_t PROJECTILE_FLIGHT_TICKS = 180;

/// How often a hazard pool bites whoever stands in it, in ticks: twice a
/// second.
inline constexpr uint32_t HAZARD_BITE_INTERVAL_TICKS = 30;

/// §4.1 step 4: spawn every projectile and hazard the tick's attacks asked
/// for, in the order they were asked. One the pool has no room for is
/// dropped.
void spawnCombatEffects(ProjectilePool& projectiles, HazardPool& hazards,
                        const CombatEffects& effects);

/// §4.1 step 5: move every projectile one tick. The first opposing body
/// its step reaches — before any box — is hit and it is gone; one a box
/// stops, or that runs out of flight, is gone too.
void stepProjectiles(ProjectilePool& projectiles, const CombatScene& scene);

/// §4.1 step 5: age every hazard one tick, biting the opposing bodies in
/// each on its bite ticks, and drying up the ones out of time.
void stepHazards(HazardPool& hazards, const CombatScene& scene);

/// Turn every blast in the scene's effects into hits on everyone it
/// reaches, in the order the blasts were set off, and clear the blasts.
void resolveBlasts(const CombatScene& scene);

/// Destroy the projectiles marked for it.
void compactProjectiles(ProjectilePool& projectiles);

/// Destroy the hazards marked for it.
void compactHazards(HazardPool& hazards);

/// Fold every projectile's state into a tick hash section.
void hashProjectiles(const ProjectilePool& projectiles,
                     sim::StateHasher& hasher);

/// Fold every hazard's state into a tick hash section.
void hashHazards(const HazardPool& hazards, sim::StateHasher& hasher);

}  // namespace eng::game
