#pragma once

/// @file tick-input.h
/// @brief Every player's input for one tick.
/// @par Threading
/// A value type.

#include <array>
#include <cstddef>
#include <engine/sim/player-input.h>

namespace eng::sim {

/// Players in a session, at most (Game REQUIREMENTS §8).
inline constexpr std::size_t MAX_PLAYERS = 4;

/// The complete input a tick runs on. Together with the initial state and
/// the seed, a sequence of these is the whole of what the simulation is a
/// function of (ADR-002).
struct TickInput {
  /// Input per player slot. Slots at or beyond the session's player count
  /// stay zero.
  std::array<PlayerInput, MAX_PLAYERS> players{};

  /// Tick inputs are equal when every player's input is.
  bool operator==(const TickInput&) const = default;
};

}  // namespace eng::sim
