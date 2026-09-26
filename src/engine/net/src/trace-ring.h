#pragma once

/// @file trace-ring.h
/// @brief A peer's recent tick hashes, kept for a desync trace.
/// @par Threading
/// Pure functions over the caller's ring.

#include <cstdint>
#include <deque>
#include <engine/net/net-trace.h>
#include <engine/sim/tick-hash.h>

namespace eng::net {

/// Keep @p hash at the back of @p ring, forgetting the oldest past
/// `NET_TRACE_TICKS`.
void keepTraced(const sim::TickHash& hash, std::deque<sim::TickHash>& ring);

/// @p ring as run @p run's trace: every hash, oldest first, and the section
/// names of the newest.
[[nodiscard]] NetTrace traceOf(uint16_t run,
                               const std::deque<sim::TickHash>& ring);

}  // namespace eng::net
