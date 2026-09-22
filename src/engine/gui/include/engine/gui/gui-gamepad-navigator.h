#pragma once

/// @file gui-gamepad-navigator.h
/// @brief A pad's d-pad, stick and face buttons as menu navigation.
/// @par Threading
/// Main-thread-only.

#include "gui-nav-command.h"

#include <engine/input/gamepad-family.h>
#include <engine/input/gamepad-state.h>
#include <optional>
#include <vector>

namespace eng {

/// Turns what a pad reads each frame into the `GuiNavCommand`s a menu
/// runs on: the d-pad or left stick moves, the shoulders step through
/// focus order, and the face buttons confirm and cancel by the pad's own
/// convention (`gui-nav-buttons.h`) — bottom confirms, right cancels, and
/// the other way round on a Nintendo pad, where the right one is A.
///
/// A direction held repeats, after a pause long enough that one press is
/// one step: a player holds the d-pad to run down a list or along a
/// slider. Buttons act on the press only. Frame time, not the simulation
/// tick — menus are presentation.
class GuiGamepadNavigator {
public:
  /// The commands @p pad, a @p family pad, asks for, @p dt_seconds after
  /// the last call; none, and everything forgotten, when @p pad is null.
  [[nodiscard]] std::vector<GuiNavCommand>
  update(const input::GamepadState* pad, input::GamepadFamily family,
         float dt_seconds);

  /// Start over from @p pad as it reads now — or from nothing — treating
  /// whatever is already down as handled: the South press that opened a
  /// menu must not also confirm its first row.
  void reset(const input::GamepadState* pad);

private:
  /// Emit into @p out the direction @p pad holds, when it is new or due
  /// to repeat.
  void stepDirection(const input::GamepadState& pad, float dt_seconds,
                     std::vector<GuiNavCommand>& out);

  /// Emit into @p out a command for each face or shoulder button that
  /// went down on @p pad, a @p family pad.
  void pressButtons(const input::GamepadState& pad, input::GamepadFamily family,
                    std::vector<GuiNavCommand>& out) const;

  /// What the pad read last frame, for presses.
  input::GamepadState before_{};
  /// The direction held, if any.
  std::optional<GuiNavCommand> held_{};
  /// Seconds until the held direction repeats.
  float repeat_in_ = 0.0f;
};

}  // namespace eng
