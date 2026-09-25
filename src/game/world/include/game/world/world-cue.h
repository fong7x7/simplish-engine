#pragma once

/// @file world-cue.h
/// @brief A cue the game logic raised, kept for presentation.
/// @par Threading
/// A value type.

#include <cstdint>
#include <engine/math/vec3.h>
#include <game/logic/logic-cue-reach.h>
#include <string>

namespace eng::game {

/// A `LogicCue` as the world keeps it until whoever presents the game
/// takes it: its names copied, and the tick it was raised on. Never state.
struct WorldCue {
  /// The tick the logic raised it on.
  uint64_t tick = 0;
  /// Where it happens, in tiles.
  Vec3 at{};
  /// The sound to play; empty for none.
  std::string sound;
  /// The effect to show; empty for none.
  std::string effect;
  /// The sound's volume.
  float gain = 1.0F;
  /// The effect's size.
  float scale = 1.0F;
  /// Where the sound is heard from.
  LogicCueReach reach = LogicCueReach::AT;
};

}  // namespace eng::game
