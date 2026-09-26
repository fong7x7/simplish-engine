#pragma once

/// @file input-action.h
/// @brief What a player can ask their character to do, independent of keys.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng::input {

/// One thing a player can hold down. Keys, buttons and stick directions
/// are bound to these (`input-bindings.h`);
/// the simulation never sees a key, only the `PlayerInput` built from the
/// actions held on the tick (Engine REQUIREMENTS §6, "deterministic
/// capture").
enum class InputAction : uint8_t {
  MOVE_UP,     ///< Up the screen: world -Y
  MOVE_DOWN,   ///< Down the screen: world +Y
  MOVE_LEFT,   ///< World -X
  MOVE_RIGHT,  ///< World +X
  FIRE,        ///< The primary weapon
  AIM_UP,      ///< Aim up the screen, as a stick does
  AIM_DOWN,    ///< Aim down the screen
  AIM_LEFT,    ///< Aim left across the screen
  AIM_RIGHT,   ///< Aim right across the screen
  PAUSE,       ///< Pause the game, or go on: the game logic decides
  COUNT,       ///< Number of actions; not an action
};

/// Number of actions a player can hold.
inline constexpr std::size_t INPUT_ACTION_COUNT =
    static_cast<std::size_t>(InputAction::COUNT);

/// The `PlayerInput::buttons` bit the fire action sets. The game reads the
/// same constant, so the two cannot disagree about which bit is which.
inline constexpr uint32_t INPUT_BUTTON_FIRE = 1U << 0U;

/// `PlayerInput::buttons`' bit for the pause button held.
inline constexpr uint32_t INPUT_BUTTON_PAUSE = 1U << 1U;

}  // namespace eng::input
