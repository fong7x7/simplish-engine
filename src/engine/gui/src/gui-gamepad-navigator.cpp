#include <algorithm>
#include <cmath>
#include <engine/gui/gui-gamepad-navigator.h>

namespace eng {

namespace {

  using input::GamepadAxis;
  using input::GamepadButton;

  /// Seconds a direction is held before it starts repeating, and between
  /// repeats after that.
  constexpr float REPEAT_DELAY = 0.4f;
  constexpr float REPEAT_INTERVAL = 0.12f;
  /// How far the stick must lean to count as a direction: well past drift.
  constexpr float STICK_THRESHOLD = 0.5f;

  /// One button a menu reads, and what it asks for.
  struct NavButton {
    /// The button, by position.
    GamepadButton button;
    /// The command a press of it makes.
    GuiNavCommand command;
  };

  /// South confirms and East cancels, as every platform's menus do by
  /// position; the shoulders step through focus order.
  constexpr NavButton NAV_BUTTONS[] = {
      {GamepadButton::SOUTH, GuiNavCommand::CONFIRM},
      {GamepadButton::EAST, GuiNavCommand::CANCEL},
      {GamepadButton::LEFT_SHOULDER, GuiNavCommand::PREVIOUS},
      {GamepadButton::RIGHT_SHOULDER, GuiNavCommand::NEXT},
  };

  /// The d-pad directions, in the order a diagonal press resolves them.
  constexpr NavButton DPAD[] = {
      {GamepadButton::DPAD_UP, GuiNavCommand::UP},
      {GamepadButton::DPAD_DOWN, GuiNavCommand::DOWN},
      {GamepadButton::DPAD_LEFT, GuiNavCommand::LEFT},
      {GamepadButton::DPAD_RIGHT, GuiNavCommand::RIGHT},
  };

  /// The direction the left stick leans past the threshold, along its
  /// stronger axis, or nothing.
  std::optional<GuiNavCommand> stickDirection(const input::GamepadState& pad) {
    const float x = pad.axis(GamepadAxis::LEFT_X);
    const float y = pad.axis(GamepadAxis::LEFT_Y);
    if (std::max(std::abs(x), std::abs(y)) < STICK_THRESHOLD) {
      return std::nullopt;
    }
    if (std::abs(x) > std::abs(y)) {
      return x > 0.0f ? GuiNavCommand::RIGHT : GuiNavCommand::LEFT;
    }
    return y > 0.0f ? GuiNavCommand::DOWN : GuiNavCommand::UP;
  }

  /// The direction @p pad holds: the d-pad's, or else the stick's.
  std::optional<GuiNavCommand> heldDirection(const input::GamepadState& pad) {
    for (const NavButton& entry : DPAD) {
      if (pad.held(entry.button)) {
        return entry.command;
      }
    }
    return stickDirection(pad);
  }

}  // namespace

std::vector<GuiNavCommand>
GuiGamepadNavigator::update(const input::GamepadState* pad, float dt_seconds) {
  std::vector<GuiNavCommand> out;
  if (pad == nullptr) {
    reset(nullptr);
    return out;
  }
  stepDirection(*pad, dt_seconds, out);
  pressButtons(*pad, out);
  before_ = *pad;
  return out;
}

void GuiGamepadNavigator::reset(const input::GamepadState* pad) {
  before_ = pad != nullptr ? *pad : input::GamepadState{};
  held_ = pad != nullptr ? heldDirection(*pad) : std::nullopt;
  repeat_in_ = REPEAT_DELAY;
}

void GuiGamepadNavigator::stepDirection(const input::GamepadState& pad,
                                        float dt_seconds,
                                        std::vector<GuiNavCommand>& out) {
  const std::optional<GuiNavCommand> direction = heldDirection(pad);
  if (direction != held_) {
    held_ = direction;
    repeat_in_ = REPEAT_DELAY;
    if (direction) {
      out.push_back(*direction);
    }
    return;
  }
  repeat_in_ -= dt_seconds;
  if (direction && repeat_in_ <= 0.0f) {
    out.push_back(*direction);
    // One repeat per frame at most: a long frame is not a burst of steps.
    repeat_in_ = std::max(repeat_in_ + REPEAT_INTERVAL, REPEAT_INTERVAL * 0.5f);
  }
}

void GuiGamepadNavigator::pressButtons(const input::GamepadState& pad,
                                       std::vector<GuiNavCommand>& out) const {
  for (const NavButton& entry : NAV_BUTTONS) {
    if (pad.held(entry.button) && !before_.held(entry.button)) {
      out.push_back(entry.command);
    }
  }
}

}  // namespace eng
