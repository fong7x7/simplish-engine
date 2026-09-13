#pragma once

/// @file combat-cue.h
/// @brief One moment of a fight, reported for whatever shows or sounds it.
/// @par Threading
/// A value type, and a function over a caller's list.

#include <engine/math/vec2.h>
#include <engine/math/vec3.h>
#include <game/combat/combat-cue-kind.h>
#include <game/content/faction.h>
#include <vector>

namespace eng::game {

/// Something a tick's combat did that presentation wants to know about — a
/// shot fired, a hit, a blast — noted where it happened.
///
/// A cue is an output of the tick, never an input to one: no phase reads
/// cues back, they are not state, and they are never hashed (ADR-002:
/// particles, audio and screen shake read the simulation and never write
/// it). A world lists a tick's cues in the order they happened and empties
/// the list when the next tick starts, so whoever steps it reads them
/// after each tick.
struct CombatCue {
  /// What happened.
  CombatCueKind kind = CombatCueKind::SHOT_FIRED;
  /// Where, in tiles; z is the height it happened at — a projectile's
  /// flight height for a shot, the floor for a blast.
  Vec3 at{};
  /// The shot's travel for one tick, which says which way it was going;
  /// zero for a blast.
  Vec2 heading{};
  /// A blast's radius, in tiles; zero for a shot.
  float radius = 0.0F;
  /// The side that fired a shot. A blast hurts every side, and is given
  /// the hostile one.
  Faction side = Faction::HOSTILE;
};

/// Append @p cue to @p cues if there is room reserved for it, and drop it
/// if not. A tick must not allocate (Engine §7), and a cue is presentation
/// — losing one in a crowded tick loses a spark, never a hit — so the list
/// is never grown here.
void cueCombat(std::vector<CombatCue>& cues, const CombatCue& cue);

}  // namespace eng::game
