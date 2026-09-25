#pragma once

/// @file logic-cue.h
/// @brief A sound or an effect the game logic asks to be played.
/// @par Threading
/// A value type; its views need live only for the `cue` call.

#include <engine/math/vec3.h>
#include <game/logic/logic-cue-reach.h>
#include <string_view>

namespace eng::game {

/// Something for players to see or hear, raised by the logic: what
/// `GameLogicWorld::cue` takes. Presentation only — the tick never reads a
/// cue back, and it is never hashed — so a cue can do nothing to the game.
///
/// Sounds and effects are named as a project's animation events name them
/// (docs/game/sdk.md): a sound is a slot (`combat.blast`, `step.boots.wood`)
/// or a WAV or Ogg file under `assets/` (`sounds/horn.wav`); an effect is a
/// particle preset (`smoke`, `fireball`, `wall_sparks`, …) or a whole
/// combat effect by its cue's name (`combat.blast`). Either may be empty.
struct LogicCue {
  /// Where it happens, in tiles.
  Vec3 at{};
  /// The sound to play; empty for none.
  std::string_view sound{};
  /// The effect to show; empty for none.
  std::string_view effect{};
  /// The sound's volume: 1 as recorded.
  float gain = 1.0F;
  /// The effect's size: 1 as authored.
  float scale = 1.0F;
  /// Where the sound is heard from.
  LogicCueReach reach = LogicCueReach::AT;
};

}  // namespace eng::game
