#include <array>
#include <catch2/catch_test_macros.hpp>
#include <engine/input/gamepad-seats.h>

using namespace eng::input;

namespace {

/// Pad @p device, with South held when @p pressed says so.
GamepadReading pad(uint64_t device, int pressed) {
  GamepadReading reading{device, {}};
  if (pressed != 0) {
    reading.state.press(GamepadButton::SOUTH);
  }
  return reading;
}

}  // namespace

TEST_CASE("a pad takes a seat when it is first touched, not when plugged in") {
  GamepadSet pads;
  GamepadSeats seats;
  pads.update(std::array{pad(7, 0), pad(9, 0)});
  seats.update(pads);
  REQUIRE_FALSE(seats.occupied(0));

  pads.update(std::array{pad(7, 0), pad(9, 1)});
  seats.update(pads);
  REQUIRE(seats.device(0) == 9U);

  pads.update(std::array{pad(7, 1), pad(9, 1)});
  seats.update(pads);
  REQUIRE(seats.device(1) == 7U);
  REQUIRE(seats.seatOf(7) == 1U);
}

TEST_CASE("unplugging frees the seat, and the next pad touched takes it") {
  GamepadSet pads;
  GamepadSeats seats;
  pads.update(std::array{pad(1, 1)});
  seats.update(pads);
  pads.update(std::array{pad(1, 1), pad(2, 1)});
  seats.update(pads);
  REQUIRE(seats.device(1) == 2U);

  pads.update(std::array{pad(2, 1)});
  seats.update(pads);
  REQUIRE_FALSE(seats.occupied(0));
  REQUIRE(seats.device(1) == 2U);

  pads.update(std::array{pad(2, 1), pad(3, 1)});
  seats.update(pads);
  REQUIRE(seats.device(0) == 3U);
}

TEST_CASE("a fifth pad waits for a seat") {
  GamepadSet pads;
  GamepadSeats seats;
  pads.update(
      std::array{pad(1, 1), pad(2, 1), pad(3, 1), pad(4, 1), pad(5, 1)});
  seats.update(pads);
  REQUIRE(seats.device(3) == 4U);
  REQUIRE_FALSE(seats.seatOf(5).has_value());
}
