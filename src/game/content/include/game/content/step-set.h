#pragma once

/// @file step-set.h
/// @brief What a character's feet sound like.
/// @par Threading
/// A value type.

#include <array>
#include <cstddef>
#include <cstdint>

namespace eng::game {

/// The kind of feet a character walks on, which with the surface under
/// them picks the sound each step makes (`footstep-sounds.h`).
///
/// A closed set, as a faction is: a project records the sounds, not new
/// kinds of feet. Presentation — the simulation never reads it.
enum class StepSet : uint8_t {
  /// Anybody the content says nothing about: a plain shoe.
  DEFAULT,
  /// Hard soles: soldiers, knights.
  BOOTS,
  /// Soft and quiet: bare feet, paws.
  BARE,
  /// Quick and clicking: claws, chitin, small skittering things.
  CLAWS,
  /// Slow and deep: brutes, machines, anything that shakes the floor.
  HEAVY,
};

/// How many step sets there are.
inline constexpr size_t STEP_SET_COUNT = 5;

/// Every step set, in the order a choice row steps through them.
inline constexpr std::array<StepSet, STEP_SET_COUNT> ALL_STEP_SETS{
    StepSet::DEFAULT, StepSet::BOOTS, StepSet::BARE, StepSet::CLAWS,
    StepSet::HEAVY};

}  // namespace eng::game
