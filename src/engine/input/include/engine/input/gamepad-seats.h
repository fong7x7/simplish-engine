#pragma once

/// @file gamepad-seats.h
/// @brief Which pad plays as which player, for a couch full of them.
/// @par Threading
/// Main-thread-only.

#include <array>
#include <cstdint>
#include <engine/input/gamepad-set.h>
#include <engine/sim/tick-input.h>
#include <optional>

namespace eng::input {

/// Pads assigned to local players: seat 0 is player 1, seat 1 player 2,
/// and so on. A pad takes the lowest free seat the first time it is
/// touched — picking it up and pressing anything is how a player joins —
/// keeps it while it stays connected, and frees it when it is unplugged,
/// so plugging it back in and pressing a button sits it down again.
///
/// Presentation-side, like the pads themselves: which pad is in which seat
/// decides whose `PlayerInput` a pad makes, and only those inputs reach
/// the simulation.
class GamepadSeats {
public:
  /// Seat and unseat pads by what @p pads saw in its last update.
  void update(const GamepadSet& pads);

  /// The pad in @p seat, or nothing when it is empty.
  [[nodiscard]] std::optional<uint64_t> device(uint8_t seat) const;

  /// Which seat pad @p device is in, or nothing.
  [[nodiscard]] std::optional<uint8_t> seatOf(uint64_t device) const;

  /// Whether @p seat has a pad in it.
  [[nodiscard]] bool occupied(uint8_t seat) const;

  /// Empty every seat.
  void clear() { seats_ = {}; }

private:
  /// The pad in each seat, if any.
  std::array<std::optional<uint64_t>, sim::MAX_PLAYERS> seats_{};
};

}  // namespace eng::input
