#pragma once

/// @file combat-rumble.h
/// @brief What a fight feels like in a player's hands.
/// @par Threading
/// Pure functions; presentation, run after the tick that cued it.
///
/// The rumble counterpart of `combat-fx.h`: the kick of a player's own
/// shot, the thump of a blast that scales with how close it went off, the
/// jolt of a hit taken. Built in for now; when feedback becomes content,
/// this is the table the content replaces.

#include <cstdint>
#include <engine/input/gamepad-rumble.h>
#include <engine/math/vec3.h>
#include <game/combat/combat-cue.h>
#include <span>

namespace eng::game {

/// The rumble @p cue gives the player standing at @p player: a shot they
/// fired kicks the light motor and the trigger, a blast thumps the heavy
/// one harder the closer it was, and anything else — a stranger's shot, a
/// spark off a far wall — is nothing.
[[nodiscard]] input::GamepadRumble combatCueRumble(const CombatCue& cue,
                                                   Vec3 player);

/// The strongest rumble any of @p cues gives the player at @p player.
[[nodiscard]] input::GamepadRumble
combatCuesRumble(std::span<const CombatCue> cues, Vec3 player);

/// The jolt of losing @p lost health segments of @p max_health: harder
/// the bigger a share of the bar went. Nothing for no loss.
[[nodiscard]] input::GamepadRumble hurtRumble(uint16_t lost,
                                              uint16_t max_health);

}  // namespace eng::game
