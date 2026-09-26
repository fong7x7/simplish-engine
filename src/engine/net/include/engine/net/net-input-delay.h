#pragma once

/// @file net-input-delay.h
/// @brief The input delay a round trip calls for.
/// @par Threading
/// Pure functions.

#include <cstdint>

namespace eng::net {

/// The least delay a server chooses from a measurement: two ticks, the low
/// end of ADR-005's default, however fast the network.
inline constexpr uint8_t NET_MIN_MEASURED_DELAY = 2;

/// The most input delay a session may have: half a second, well inside the
/// input queue's reach (`sim::INPUT_QUEUE_TICKS`).
inline constexpr uint8_t NET_MAX_INPUT_DELAY = 30;

/// The input delay, in ticks, for a session whose worst round trip to the
/// server is @p round_trip_ms (ADR-005: "tuned against measured session
/// RTT"). An input goes half a round trip to the server and its frame half
/// a round trip back to every client, so a delay covering the worst whole
/// round trip, plus a tick for where in a tick things land, stalls nobody.
/// Between `NET_MIN_MEASURED_DELAY` and `NET_MAX_INPUT_DELAY`.
[[nodiscard]] uint8_t inputDelayForRoundTrip(uint32_t round_trip_ms);

}  // namespace eng::net
