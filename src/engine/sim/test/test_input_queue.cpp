#include <catch2/catch_test_macros.hpp>
#include <engine/sim/input-queue.h>

using eng::sim::INPUT_QUEUE_TICKS;
using eng::sim::InputQueue;
using eng::sim::InputSubmitResult;
using eng::sim::PlayerInput;

namespace {

PlayerInput pressing(uint32_t buttons) {
  PlayerInput input;
  input.buttons = buttons;
  return input;
}

}  // namespace

TEST_CASE("InputQueue gives a solo player's input straight back") {
  InputQueue queue(1);
  REQUIRE(queue.submit(0, 0, pressing(3)) == InputSubmitResult::ACCEPTED);
  const auto input = queue.take();
  REQUIRE(input.has_value());
  CHECK(input->players[0].buttons == 3);
  CHECK(queue.nextTick() == 1);
}

TEST_CASE("InputQueue waits for every player before giving up a tick") {
  InputQueue queue(2);
  REQUIRE(queue.submit(0, 0, pressing(1)) == InputSubmitResult::ACCEPTED);
  CHECK_FALSE(queue.take().has_value());
  CHECK(queue.nextTick() == 0);
  REQUIRE(queue.submit(1, 0, pressing(2)) == InputSubmitResult::ACCEPTED);
  const auto input = queue.take();
  REQUIRE(input.has_value());
  CHECK(input->players[0].buttons == 1);
  CHECK(input->players[1].buttons == 2);
}

TEST_CASE("InputQueue takes inputs submitted ahead, in tick order") {
  InputQueue queue(1);
  REQUIRE(queue.submit(0, 2, pressing(30)) == InputSubmitResult::ACCEPTED);
  REQUIRE(queue.submit(0, 1, pressing(20)) == InputSubmitResult::ACCEPTED);
  CHECK_FALSE(queue.take().has_value());
  REQUIRE(queue.submit(0, 0, pressing(10)) == InputSubmitResult::ACCEPTED);
  CHECK(queue.take()->players[0].buttons == 10);
  CHECK(queue.take()->players[0].buttons == 20);
  CHECK(queue.take()->players[0].buttons == 30);
}

TEST_CASE("InputQueue rejects what it cannot use") {
  InputQueue queue(2);
  CHECK(queue.submit(2, 0, {}) == InputSubmitResult::UNKNOWN_PLAYER);
  CHECK(queue.submit(0, INPUT_QUEUE_TICKS, {}) ==
        InputSubmitResult::TOO_FAR_AHEAD);
  REQUIRE(queue.submit(0, 0, {}) == InputSubmitResult::ACCEPTED);
  CHECK(queue.submit(0, 0, {}) == InputSubmitResult::DUPLICATE);
  REQUIRE(queue.submit(1, 0, {}) == InputSubmitResult::ACCEPTED);
  REQUIRE(queue.take().has_value());
  CHECK(queue.submit(0, 0, {}) == InputSubmitResult::ALREADY_TAKEN);
}

TEST_CASE("InputQueue leaves slots beyond the player count at zero") {
  InputQueue queue(1);
  REQUIRE(queue.submit(0, 0, pressing(1)) == InputSubmitResult::ACCEPTED);
  const auto input = queue.take();
  REQUIRE(input.has_value());
  CHECK(input->players[1] == PlayerInput{});
  CHECK(input->players[3] == PlayerInput{});
}

TEST_CASE("InputQueue keeps working as its ring wraps") {
  InputQueue queue(1);
  for (uint64_t tick = 0; tick < INPUT_QUEUE_TICKS * 3; ++tick) {
    const auto buttons = static_cast<uint32_t>(tick);
    REQUIRE(queue.submit(0, tick, pressing(buttons)) ==
            InputSubmitResult::ACCEPTED);
    const auto input = queue.take();
    REQUIRE(input.has_value());
    REQUIRE(input->players[0].buttons == buttons);
  }
}
