#pragma once

/// @file test-input.h
/// @brief What a logic test has a player do.
/// @par Threading
/// A value type.

namespace eng::game::sdk {

/// A player's controls, held by a logic test from one tick to the next.
/// The same controls a person or `send_input` gives: stick and aim in world
/// axes, -1 to 1, and the trigger.
struct TestInput {
  /// Stick along world X.
  float move_x = 0.0F;
  /// Stick along world Y.
  float move_y = 0.0F;
  /// Aim along world X; an aim of 0, 0 keeps the last.
  float aim_x = 0.0F;
  /// Aim along world Y.
  float aim_y = 0.0F;
  /// Whether fire is held, as the trigger or the left mouse button.
  bool fire = false;
};

}  // namespace eng::game::sdk
