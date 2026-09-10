#pragma once

/// @file held-actions.h
/// @brief Which actions a player is holding down right now.
/// @par Threading
/// Main-thread-only.

#include <cstdint>
#include <engine/input/input-action.h>

namespace eng::input {

/// The actions currently held, updated as presses and releases arrive and
/// read once per simulation tick.
///
/// Held state rather than a queue of events: the simulation samples input
/// at 60 Hz, and what it needs on a tick is what is down on that tick, not
/// how many times something went down and up between two of them.
class HeldActions {
public:
  /// Mark @p action held.
  void press(InputAction action);

  /// Mark @p action released.
  void release(InputAction action);

  /// Release everything — for when the window loses focus and the releases
  /// of whatever was held will never arrive.
  void releaseAll() { bits_ = 0; }

  /// Whether @p action is held.
  [[nodiscard]] bool held(InputAction action) const;

private:
  /// One bit per `InputAction`, set while it is held.
  uint32_t bits_ = 0;
};

}  // namespace eng::input
