#include <algorithm>
#include <engine/input/gamepad-rumble.h>

namespace eng::input {

bool isRumbling(const GamepadRumble& rumble) {
  const float strongest = std::max(
      {rumble.low, rumble.high, rumble.left_trigger, rumble.right_trigger});
  return rumble.seconds > 0.0F && strongest > 0.0F;
}

GamepadRumble strongerRumble(const GamepadRumble& a, const GamepadRumble& b) {
  return {std::max(a.low, b.low), std::max(a.high, b.high),
          std::max(a.left_trigger, b.left_trigger),
          std::max(a.right_trigger, b.right_trigger),
          std::max(a.seconds, b.seconds)};
}

}  // namespace eng::input
