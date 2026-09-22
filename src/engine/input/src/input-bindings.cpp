#include <algorithm>
#include <engine/input/input-bindings.h>

namespace eng::input {

namespace {

  /// The most of a stick's travel a deadzone may take: past this there is
  /// no range left to steer with.
  constexpr float DEADZONE_MAX = 0.95F;

  /// One default pad binding.
  struct DefaultBinding {
    /// The action bound.
    InputAction action = InputAction::MOVE_UP;
    /// The control bound to it.
    InputSource source;
  };

  using A = InputAction;
  using S = InputSource;

  /// The left stick and d-pad move, the right stick aims, and the right
  /// trigger or shoulder fires — the twin-stick layout, on every pad.
  constexpr DefaultBinding DEFAULT_PAD[] = {
      {A::MOVE_UP, S::negative(GamepadAxis::LEFT_Y)},
      {A::MOVE_UP, S::button(GamepadButton::DPAD_UP)},
      {A::MOVE_DOWN, S::positive(GamepadAxis::LEFT_Y)},
      {A::MOVE_DOWN, S::button(GamepadButton::DPAD_DOWN)},
      {A::MOVE_LEFT, S::negative(GamepadAxis::LEFT_X)},
      {A::MOVE_LEFT, S::button(GamepadButton::DPAD_LEFT)},
      {A::MOVE_RIGHT, S::positive(GamepadAxis::LEFT_X)},
      {A::MOVE_RIGHT, S::button(GamepadButton::DPAD_RIGHT)},
      {A::FIRE, S::positive(GamepadAxis::RIGHT_TRIGGER)},
      {A::FIRE, S::button(GamepadButton::RIGHT_SHOULDER)},
      {A::AIM_UP, S::negative(GamepadAxis::RIGHT_Y)},
      {A::AIM_DOWN, S::positive(GamepadAxis::RIGHT_Y)},
      {A::AIM_LEFT, S::negative(GamepadAxis::RIGHT_X)},
      {A::AIM_RIGHT, S::positive(GamepadAxis::RIGHT_X)},
  };

  /// @p deadzone held to a range that leaves something to steer with.
  float saneDeadzone(float deadzone) {
    return deadzone == deadzone ? std::clamp(deadzone, 0.0F, DEADZONE_MAX)
                                : 0.0F;
  }

}  // namespace

void InputBindings::bind(InputAction action, InputSource source) {
  std::vector<InputSource>& bound = sources_[static_cast<std::size_t>(action)];
  if (std::ranges::find(bound, source) == bound.end()) {
    bound.push_back(source);
  }
}

void InputBindings::unbind(InputAction action, InputSource source) {
  std::erase(sources_[static_cast<std::size_t>(action)], source);
}

void InputBindings::clear(InputAction action) {
  sources_[static_cast<std::size_t>(action)].clear();
}

std::span<const InputSource> InputBindings::sources(InputAction action) const {
  return sources_[static_cast<std::size_t>(action)];
}

std::vector<InputAction> InputBindings::actionsFor(InputSource source) const {
  std::vector<InputAction> actions;
  for (std::size_t i = 0; i < INPUT_ACTION_COUNT; ++i) {
    if (std::ranges::find(sources_[i], source) != sources_[i].end()) {
      actions.push_back(static_cast<InputAction>(i));
    }
  }
  return actions;
}

void InputBindings::setDeadzones(const GamepadDeadzones& deadzones) {
  deadzones_ = {saneDeadzone(deadzones.left_stick),
                saneDeadzone(deadzones.right_stick),
                saneDeadzone(deadzones.trigger)};
}

InputBindings defaultGamepadBindings() {
  InputBindings bindings;
  for (const DefaultBinding& binding : DEFAULT_PAD) {
    bindings.bind(binding.action, binding.source);
  }
  return bindings;
}

}  // namespace eng::input
