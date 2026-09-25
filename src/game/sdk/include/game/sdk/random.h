#pragma once

/// @file random.h
/// @brief Dice, drawn from the run's own logic stream.
/// @par Threading
/// Draws on the world; call inside the logic's tick.

#include <cstdint>
#include <game/logic/game-logic-world.h>
#include <span>

namespace eng::game::sdk {

/// True @p permille times in a thousand.
[[nodiscard]] bool chance(GameLogicWorld& world, uint32_t permille);

/// A whole number from @p low to @p high, both included; @p low when
/// @p high is not above it.
[[nodiscard]] int32_t between(GameLogicWorld& world, int32_t low, int32_t high);

/// One of @p choices, each as likely; @p choices must not be empty.
template <typename T>
[[nodiscard]] const T& pick(GameLogicWorld& world, std::span<const T> choices) {
  return choices[world.random(static_cast<uint32_t>(choices.size()))];
}

}  // namespace eng::game::sdk
