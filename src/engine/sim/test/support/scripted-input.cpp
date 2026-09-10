#include "support/scripted-input.h"

namespace eng::sim::testing {

namespace {

  /// Full-scale input axis value.
  constexpr int32_t AXIS_MAX = 32767;

  /// A value sweeping back and forth across the axis range.
  int16_t sweep(uint64_t tick, uint64_t step) {
    const auto phase = static_cast<int32_t>((tick * step) % (2U * AXIS_MAX));
    return static_cast<int16_t>(phase - AXIS_MAX);
  }

}  // namespace

TickInput scriptedInput(uint64_t tick) {
  TickInput input;
  PlayerInput& first = input.players[0];
  first.buttons = tick % 7 == 0 ? 1U : 0U;
  first.aim_x = sweep(tick, 997);
  first.aim_y = sweep(tick + 50, 613);
  PlayerInput& second = input.players[1];
  second.move_x = tick % 120 < 60 ? int16_t{AXIS_MAX} : int16_t{-AXIS_MAX};
  second.buttons = tick % 11 == 0 ? 1U : 0U;
  second.aim_x = sweep(tick, 401);
  return input;
}

}  // namespace eng::sim::testing
