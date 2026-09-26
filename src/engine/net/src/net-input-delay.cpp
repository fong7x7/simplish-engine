#include <algorithm>
#include <engine/core/fixed-step-clock.h>
#include <engine/net/net-input-delay.h>

namespace eng::net {

uint8_t inputDelayForRoundTrip(uint32_t round_trip_ms) {
  constexpr uint64_t MS_PER_SECOND = 1000;
  // Whole ticks the round trip spans, rounded up, and one for phase.
  const uint64_t ticks =
      ((uint64_t{round_trip_ms} * TICK_RATE_HZ) + MS_PER_SECOND - 1) /
          MS_PER_SECOND +
      1;
  return static_cast<uint8_t>(
      std::clamp<uint64_t>(ticks, NET_MIN_MEASURED_DELAY, NET_MAX_INPUT_DELAY));
}

}  // namespace eng::net
