#include "host-logic-test.h"

#include <algorithm>
#include <cmath>
#include <engine/input/input-action.h>
#include <engine/input/player-input-builder.h>

namespace eng::editor {

namespace {

  /// @p value, -1 to 1, as an input axis.
  int16_t axis(float value) {
    return static_cast<int16_t>(
        std::lround(std::clamp(value, -1.0F, 1.0F) * input::INPUT_AXIS_MAX));
  }

  /// @p input as the tick reads it.
  sim::PlayerInput quantised(const game::sdk::TestInput& input) {
    return {axis(input.move_x), axis(input.move_y), axis(input.aim_x),
            axis(input.aim_y), input.fire ? input::INPUT_BUTTON_FIRE : 0U};
  }

}  // namespace

HostLogicTest::HostLogicTest(const game::GameSetup& setup,
                             const game::GameContent& content,
                             game::GameLogicFactory logic)
  : logic_(logic), world_(setup, content, logic_.get()),
    simulation_(world_, sim::TickHashing::OFF), view_(world_.readView(0)) {}

void HostLogicTest::run(uint64_t ticks) {
  for (uint64_t i = 0; i < ticks && !world_.runOver(); ++i) {
    (void)simulation_.step(input_);
    for (std::string& line : world_.takeLogicLog()) {
      log_.push_back(std::move(line));
    }
  }
  view_ = world_.readView(
      simulation_.nextTick() == 0 ? 0 : simulation_.nextTick() - 1);
}

void HostLogicTest::hold(uint8_t slot, const game::sdk::TestInput& input) {
  if (slot < input_.players.size()) {
    input_.players[slot] = quantised(input);
  }
}

const game::GameLogicWorld& HostLogicTest::world() const {
  return *view_;
}

bool HostLogicTest::logged(std::string_view text) const {
  return std::ranges::any_of(log_, [text](const std::string& line) {
    return line.find(text) != std::string::npos;
  });
}

void HostLogicTest::record(const game::sdk::TestCheck& check) {
  if (!check.passed) {
    failures_.push_back(
        {std::string(check.file), check.line, std::string(check.what)});
  }
}

}  // namespace eng::editor
