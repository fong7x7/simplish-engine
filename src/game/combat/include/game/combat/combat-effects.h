#pragma once

/// @file combat-effects.h
/// @brief What a tick's attacks asked for, until the phases that carry it
/// out.
/// @par Threading
/// Main-thread-only; filled and emptied within one tick.

#include <game/combat/blast-event.h>
#include <game/combat/damage-event.h>
#include <game/combat/hazard-request.h>
#include <game/combat/shot-request.h>
#include <vector>

namespace eng::game {

/// The effects buffer: actors append what their attacks do, and later
/// phases of the same tick carry it out — shots and hazards spawned in
/// weapon fire, blasts and damage applied in the damage phase. Nothing an
/// attack does writes simulation state where it happens, so its result is
/// the same wherever in a pass the attacker came.
///
/// Emptied by the damage phase, so it is empty between ticks and is not
/// state; reserved when the world is built, so appending does not allocate.
struct CombatEffects {
  /// Hits to apply, in the order they happened.
  std::vector<DamageEvent> damage{};
  /// Explosions to resolve into hits.
  std::vector<BlastEvent> blasts{};
  /// Projectiles to spawn.
  std::vector<ShotRequest> shots{};
  /// Hazard pools to spawn.
  std::vector<HazardRequest> hazards{};
};

/// Empty every list of @p effects, keeping what they reserved.
void clearCombatEffects(CombatEffects& effects);

}  // namespace eng::game
