#pragma once

/// @file input-bindings.h
/// @brief Which keys, buttons and axes ask for which action.
/// @par Threading
/// Main-thread-only.

#include <array>
#include <engine/input/gamepad-deadzones.h>
#include <engine/input/input-action.h>
#include <engine/input/input-source.h>
#include <span>
#include <vector>

namespace eng::input {

/// A player's control scheme: for each action, the controls that ask for
/// it, and how much of a stick's travel to ignore.
///
/// A control may be bound to several actions and an action to several
/// controls. Rebinding is editing this and saving it
/// (`input-bindings-json.h`); nothing below the platform layer knows what
/// a key symbol means, only that it is one.
class InputBindings {
public:
  /// Bind @p source to @p action, as well as whatever it already has. A
  /// binding already there is not added twice.
  void bind(InputAction action, InputSource source);

  /// Take @p source off @p action, if it is on it.
  void unbind(InputAction action, InputSource source);

  /// Take every control off @p action.
  void clear(InputAction action);

  /// The controls bound to @p action, in the order they were bound.
  [[nodiscard]] std::span<const InputSource> sources(InputAction action) const;

  /// The actions @p source is bound to, in action order.
  [[nodiscard]] std::vector<InputAction> actionsFor(InputSource source) const;

  /// How much of each stick and trigger's travel is ignored.
  [[nodiscard]] const GamepadDeadzones& deadzones() const { return deadzones_; }

  /// Set how much of each stick and trigger's travel is ignored, each
  /// clamped to [0, 0.95].
  void setDeadzones(const GamepadDeadzones& deadzones);

private:
  /// Each `InputAction`'s controls.
  std::array<std::vector<InputSource>, INPUT_ACTION_COUNT> sources_{};
  /// The pad's deadzones.
  GamepadDeadzones deadzones_{};
};

/// The default pad scheme, the same on every pad: the left stick and d-pad
/// move, the right stick aims, and the right trigger or right shoulder
/// fires. No keys — which keys exist is the platform's business, and it
/// adds its own.
[[nodiscard]] InputBindings defaultGamepadBindings();

}  // namespace eng::input
