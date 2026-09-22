#include <algorithm>
#include <engine/input/action-values.h>

namespace eng::input {

namespace {

  /// Strength past which an on-or-off action counts as pressed.
  constexpr float PRESS_THRESHOLD = 0.5F;

}  // namespace

ActionValues::ActionValues(const HeldActions& held) {
  for (std::size_t i = 0; i < INPUT_ACTION_COUNT; ++i) {
    if (held.held(static_cast<InputAction>(i))) {
      values_[i] = 1.0F;
    }
  }
}

void ActionValues::offer(InputAction action, float value) {
  float& slot = values_[static_cast<std::size_t>(action)];
  slot = std::max(slot, std::clamp(value, 0.0F, 1.0F));
}

bool ActionValues::pressed(InputAction action) const {
  return value(action) >= PRESS_THRESHOLD;
}

}  // namespace eng::input
