#pragma once

/// @file ticks.h
/// @brief Time, in the only unit a simulation has: ticks.
/// @par Threading
/// Pure functions.

#include <cstdint>

namespace eng::game::sdk {

/// Ticks in a second: a tick is always 1/60 s (ADR-002).
inline constexpr uint64_t TICKS_PER_SECOND = 60;

/// @p count seconds, in ticks.
[[nodiscard]] constexpr uint64_t seconds(uint64_t count) {
  return count * TICKS_PER_SECOND;
}

/// @p count minutes, in ticks.
[[nodiscard]] constexpr uint64_t minutes(uint64_t count) {
  return seconds(count * 60);
}

}  // namespace eng::game::sdk
