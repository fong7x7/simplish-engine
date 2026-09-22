#pragma once

/// @file action-values.h
/// @brief How hard each action is being asked for this frame, 0 to 1.
/// @par Threading
/// Main-thread-only.

#include <array>
#include <engine/input/held-actions.h>
#include <engine/input/input-action.h>

namespace eng::input {

/// Every action's strength this frame: 1 for a key held, anything from 0
/// to 1 for a stick or trigger part way. What `makePlayerInput` reads, so
/// a key and a stick bound to the same action reach the tick the same way.
///
/// Several controls can feed one action — a key and the stick, two pads —
/// and the strongest wins rather than their sum, so holding W and pushing
/// the stick up is not faster than either.
class ActionValues {
public:
  /// Every action at 0.
  ActionValues() = default;

  /// Every action @p held at full strength, and the rest at 0.
  explicit ActionValues(const HeldActions& held);

  /// Raise @p action to @p value if that is stronger than it already is.
  /// @p value is clamped to [0, 1].
  void offer(InputAction action, float value);

  /// How hard @p action is being asked for, 0 to 1.
  [[nodiscard]] float value(InputAction action) const {
    return values_[static_cast<std::size_t>(action)];
  }

  /// Whether @p action is pressed, for an action that is on or off: past
  /// halfway, so a trigger fires at half a pull.
  [[nodiscard]] bool pressed(InputAction action) const;

private:
  /// Each `InputAction`'s strength.
  std::array<float, INPUT_ACTION_COUNT> values_{};
};

}  // namespace eng::input
