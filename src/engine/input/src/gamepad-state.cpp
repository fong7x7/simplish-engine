#include <algorithm>
#include <engine/input/gamepad-state.h>

namespace eng::input {

namespace {

  /// The bit @p button is held in.
  uint32_t bitOf(GamepadButton button) {
    return 1U << static_cast<uint32_t>(button);
  }

  /// The lowest value @p axis can take: 0 for a trigger, -1 for a stick.
  float axisFloor(GamepadAxis axis) {
    const bool trigger =
        axis == GamepadAxis::LEFT_TRIGGER || axis == GamepadAxis::RIGHT_TRIGGER;
    return trigger ? 0.0F : -1.0F;
  }

}  // namespace

static_assert(GAMEPAD_BUTTON_COUNT <= 32, "one bit per button in a uint32_t");

void GamepadState::press(GamepadButton button) {
  buttons_ |= bitOf(button);
}

void GamepadState::release(GamepadButton button) {
  buttons_ &= ~bitOf(button);
}

bool GamepadState::held(GamepadButton button) const {
  return (buttons_ & bitOf(button)) != 0U;
}

void GamepadState::setAxis(GamepadAxis axis, float value) {
  // NaN compares false both ways, so it would pass a clamp; a pad that
  // reports one is read as resting.
  const float sane = value == value ? value : 0.0F;
  axes_[static_cast<std::size_t>(axis)] =
      std::clamp(sane, axisFloor(axis), 1.0F);
}

}  // namespace eng::input
