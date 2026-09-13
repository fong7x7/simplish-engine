#pragma once

/// @file combat-fx.h
/// @brief The effect each moment of a fight plays, and playing them.
/// @par Threading
/// Main-thread-only; presentation, run after the tick that cued it.
///
/// The one place a combat cue becomes something seen: a muzzle flash for a
/// shot fired, sparks and a flare for a shot a wall stops, a spray for one
/// that hits someone, a fireball for a blast. Built in for now; when effects
/// become content, this is the table the content replaces.

#include <engine/render-fx/fx-effect.h>
#include <engine/render-fx/fx-emit.h>
#include <engine/render-fx/fx-world.h>
#include <game/combat/combat-cue-kind.h>
#include <game/combat/combat-cue.h>
#include <span>

namespace eng::game {

/// The blast radius, in tiles, the blast effect is drawn for as it is; a
/// blast of another radius plays it scaled to match.
inline constexpr float COMBAT_FX_BLAST_RADIUS = 1.5F;

/// Lowest hue, in degrees, of the band hostile projectiles are drawn in —
/// the orange the playtest draws them in today (Engine REQUIREMENTS §5.5:
/// "a reserved hue band that no cosmetic effect may use"). No effect here
/// puts a saturated colour inside it, and `test_combat_fx` holds them to
/// that.
inline constexpr float HOSTILE_SHOT_HUE_MIN_DEGREES = 15.0F;

/// Highest hue, in degrees, of that band.
inline constexpr float HOSTILE_SHOT_HUE_MAX_DEGREES = 45.0F;

/// The effect a cue of @p kind plays.
[[nodiscard]] const FxEffect& combatCueEffect(CombatCueKind kind);

/// Where, which way and how big @p cue's effect plays: a muzzle flash
/// along the shot, sparks thrown back off a wall and up, a spray carried
/// on through whoever was hit, a blast upward and scaled by its radius.
[[nodiscard]] FxEmit combatCueEmit(const CombatCue& cue);

/// Play the effect of every cue in @p cues into @p world, in order.
void playCombatCues(FxWorld& world, std::span<const CombatCue> cues);

}  // namespace eng::game
