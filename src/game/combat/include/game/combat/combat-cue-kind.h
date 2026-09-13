#pragma once

/// @file combat-cue-kind.h
/// @brief What a combat cue says happened.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::game {

/// The moments of a fight worth seeing or hearing: what a `CombatCue`
/// reports. A closed set, so whatever turns cues into effects or sounds
/// can say what each one is and a new kind fails to compile there.
enum class CombatCueKind : uint8_t {
  /// A projectile left whoever fired it: a muzzle flash.
  SHOT_FIRED,
  /// A projectile reached someone on the other side and hurt them.
  SHOT_HIT_BODY,
  /// A projectile was stopped by the level's geometry.
  SHOT_HIT_WALL,
  /// Something went off, reaching everyone within its radius.
  BLAST,
};

/// How many kinds of cue there are.
inline constexpr uint8_t COMBAT_CUE_KIND_COUNT = 4;

}  // namespace eng::game
