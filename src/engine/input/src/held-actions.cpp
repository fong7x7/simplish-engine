#include <engine/input/held-actions.h>

namespace eng::input {

namespace {

  /// The bit @p action is held in.
  uint32_t bitOf(InputAction action) {
    return 1U << static_cast<uint32_t>(action);
  }

}  // namespace

void HeldActions::press(InputAction action) {
  bits_ |= bitOf(action);
}

void HeldActions::release(InputAction action) {
  bits_ &= ~bitOf(action);
}

bool HeldActions::held(InputAction action) const {
  return (bits_ & bitOf(action)) != 0U;
}

}  // namespace eng::input
