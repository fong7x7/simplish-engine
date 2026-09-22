#include <algorithm>
#include <array>
#include <cmath>
#include <engine/input/gamepad-actions.h>

namespace eng::input {

namespace {

  /// Every axis of @p pad after its deadzones: the sticks radially, the
  /// triggers from the bottom.
  std::array<float, GAMEPAD_AXIS_COUNT>
  deadzonedAxes(const GamepadState& pad, const GamepadDeadzones& deadzones) {
    using Axis = GamepadAxis;
    const Vec2 left = applyStickDeadzone(
        {pad.axis(Axis::LEFT_X), pad.axis(Axis::LEFT_Y)}, deadzones.left_stick);
    const Vec2 right =
        applyStickDeadzone({pad.axis(Axis::RIGHT_X), pad.axis(Axis::RIGHT_Y)},
                           deadzones.right_stick);
    return {
        left.x,
        left.y,
        right.x,
        right.y,
        applyTriggerDeadzone(pad.axis(Axis::LEFT_TRIGGER), deadzones.trigger),
        applyTriggerDeadzone(pad.axis(Axis::RIGHT_TRIGGER), deadzones.trigger)};
  }

  /// How far @p source asks, 0 to 1, on a pad whose buttons are @p pad and
  /// whose deadzoned axes are @p axes. A key is not on a pad: 0.
  float strength(InputSource source, const GamepadState& pad,
                 const std::array<float, GAMEPAD_AXIS_COUNT>& axes) {
    switch (source.kind) {
      case InputSourceKind::GAMEPAD_BUTTON:
        return pad.held(static_cast<GamepadButton>(source.code)) ? 1.0F : 0.0F;
      case InputSourceKind::GAMEPAD_AXIS_POSITIVE:
        return std::max(0.0F, axes[source.code]);
      case InputSourceKind::GAMEPAD_AXIS_NEGATIVE:
        return std::max(0.0F, -axes[source.code]);
      case InputSourceKind::KEY:
        break;
    }
    return 0.0F;
  }

  /// Whether @p source names a button or axis this engine has — a binding
  /// read from a file is checked on load, but a hand-built one may not be.
  bool onPad(InputSource source) {
    const bool button = source.kind == InputSourceKind::GAMEPAD_BUTTON;
    return button ? source.code < GAMEPAD_BUTTON_COUNT
                  : source.kind != InputSourceKind::KEY &&
                        source.code < GAMEPAD_AXIS_COUNT;
  }

}  // namespace

Vec2 applyStickDeadzone(Vec2 stick, float deadzone) {
  const float length = std::sqrt(stick.x * stick.x + stick.y * stick.y);
  if (!(length > deadzone) || deadzone >= 1.0F) {
    return {};
  }
  const float scaled = std::min(1.0F, (length - deadzone) / (1.0F - deadzone));
  return {stick.x / length * scaled, stick.y / length * scaled};
}

float applyTriggerDeadzone(float trigger, float deadzone) {
  if (!(trigger > deadzone) || deadzone >= 1.0F) {
    return 0.0F;
  }
  return std::min(1.0F, (trigger - deadzone) / (1.0F - deadzone));
}

void offerGamepad(ActionValues& values, const GamepadState& pad,
                  const InputBindings& bindings) {
  const auto axes = deadzonedAxes(pad, bindings.deadzones());
  for (std::size_t i = 0; i < INPUT_ACTION_COUNT; ++i) {
    const auto action = static_cast<InputAction>(i);
    for (const InputSource& source : bindings.sources(action)) {
      if (onPad(source)) {
        values.offer(action, strength(source, pad, axes));
      }
    }
  }
}

}  // namespace eng::input
