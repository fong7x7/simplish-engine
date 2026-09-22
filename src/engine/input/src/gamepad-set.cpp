#include <algorithm>
#include <cmath>
#include <engine/input/gamepad-set.h>

namespace eng::input {

namespace {

  /// How far a stick or trigger must go to count as the player picking a
  /// pad up: well past any drift.
  constexpr float TOUCH_THRESHOLD = 0.5F;

  /// Whether @p axis crossed the touch threshold from @p before to @p now.
  bool axisTouched(const GamepadState& before, const GamepadState& now,
                   GamepadAxis axis) {
    return std::abs(now.axis(axis)) >= TOUCH_THRESHOLD &&
           std::abs(before.axis(axis)) < TOUCH_THRESHOLD;
  }

  /// Whether the pad that went from @p before to @p now was touched.
  bool touched(const GamepadState& before, const GamepadState& now) {
    for (std::size_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i) {
      const auto button = static_cast<GamepadButton>(i);
      if (now.held(button) && !before.held(button)) {
        return true;
      }
    }
    for (std::size_t i = 0; i < GAMEPAD_AXIS_COUNT; ++i) {
      if (axisTouched(before, now, static_cast<GamepadAxis>(i))) {
        return true;
      }
    }
    return false;
  }

}  // namespace

void GamepadSet::update(std::span<const GamepadReading> readings) {
  std::vector<Pad> next;
  next.reserve(readings.size());
  std::optional<uint64_t> touched_device;
  for (const GamepadReading& reading : readings) {
    const Pad* was = find(reading.device);
    const GamepadState before = was != nullptr ? was->now : GamepadState{};
    next.push_back({reading.device, reading.state, before, reading.family});
    if (!touched_device && touched(before, reading.state)) {
      touched_device = reading.device;
    }
  }
  pads_ = std::move(next);
  chooseActive(touched_device);
}

void GamepadSet::chooseActive(std::optional<uint64_t> touched_device) {
  if (touched_device) {
    active_ = touched_device;
  } else if (!active_ || find(*active_) == nullptr) {
    // The pad in use went away, or there never was one: the first left
    // stands in until another is touched.
    active_ =
        pads_.empty() ? std::nullopt : std::optional{pads_.front().device};
  }
}

const GamepadState* GamepadSet::active() const {
  const Pad* pad = activePad();
  return pad != nullptr ? &pad->now : nullptr;
}

GamepadFamily GamepadSet::activeFamily() const {
  const Pad* pad = activePad();
  return pad != nullptr ? pad->family : GamepadFamily::GENERIC;
}

bool GamepadSet::pressed(GamepadButton button) const {
  const Pad* pad = activePad();
  return pad != nullptr && pad->now.held(button) && !pad->before.held(button);
}

const GamepadSet::Pad* GamepadSet::find(uint64_t device) const {
  const auto found = std::ranges::find(pads_, device, &Pad::device);
  return found != pads_.end() ? &*found : nullptr;
}

const GamepadSet::Pad* GamepadSet::activePad() const {
  return active_ ? find(*active_) : nullptr;
}

}  // namespace eng::input
