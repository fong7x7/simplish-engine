#pragma once

/// @file frame-pacing.h
/// @brief How many confirmed frames a client steps this render frame.
/// @par Threading
/// Pure functions.

#include <cstddef>
#include <cstdint>

namespace eng::net {

/// A client behind by `n` frames beyond what its clock owes steps
/// `n / NET_CATCH_UP_DIVISOR` of them extra a frame, rounded up: a second's
/// backlog is gone in about a fifth of a second, one tick at a time rather
/// than in one jump.
inline constexpr std::size_t NET_CATCH_UP_DIVISOR = 8;

/// The frames to step now, of @p waiting arrived, when the local clock says
/// @p due ticks are owed (ADR-013). Stepping every arrived frame at once
/// makes a rendered client jump after each stall — a burst of frames lands
/// together when the late input does. This steps what the clock owes and
/// eats into any backlog a little each frame, so motion stays even while
/// the client catches up; with nothing buffered it steps what has arrived.
/// Never more than @p waiting.
[[nodiscard]] std::size_t framesToStep(uint32_t due, std::size_t waiting);

}  // namespace eng::net
