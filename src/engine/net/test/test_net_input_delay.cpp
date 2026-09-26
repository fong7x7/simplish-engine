#include <catch2/catch_test_macros.hpp>
#include <engine/net/net-input-delay.h>

using eng::net::inputDelayForRoundTrip;
using eng::net::NET_MAX_INPUT_DELAY;
using eng::net::NET_MIN_MEASURED_DELAY;

TEST_CASE("a fast network gets the least measured delay") {
  CHECK(inputDelayForRoundTrip(0) == NET_MIN_MEASURED_DELAY);
  CHECK(inputDelayForRoundTrip(1) == NET_MIN_MEASURED_DELAY);
  CHECK(inputDelayForRoundTrip(16) == NET_MIN_MEASURED_DELAY);
}

TEST_CASE("the measured delay covers the round trip, and a tick more") {
  CHECK(inputDelayForRoundTrip(17) == 3);  // just over one tick
  CHECK(inputDelayForRoundTrip(50) == 4);  // three ticks exactly, + 1
  CHECK(inputDelayForRoundTrip(51) == 5);
  CHECK(inputDelayForRoundTrip(120) == 9);  // 7.2 ticks, rounded up, + 1
}

TEST_CASE("a slow network's delay stops at the most a session may have") {
  CHECK(inputDelayForRoundTrip(1000) == NET_MAX_INPUT_DELAY);
  CHECK(inputDelayForRoundTrip(UINT32_MAX) == NET_MAX_INPUT_DELAY);
}
