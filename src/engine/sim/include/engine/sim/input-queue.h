#pragma once

/// @file input-queue.h
/// @brief Holds player inputs until every player's input for a tick is in.
/// @par Threading
/// Main-thread-only.

#include <array>
#include <cstddef>
#include <cstdint>
#include <engine/sim/player-input.h>
#include <engine/sim/tick-input.h>
#include <optional>

namespace eng::sim {

/// Ticks ahead of the next one that an input may be submitted for — about a
/// second at 60 Hz, far beyond the 2–3 tick input delay of ADR-005.
inline constexpr std::size_t INPUT_QUEUE_TICKS = 64;

/// Why `InputQueue::submit` did or did not take an input.
enum class InputSubmitResult : uint8_t {
  ACCEPTED,        ///< Stored for its tick
  DUPLICATE,       ///< That player already has input for that tick
  ALREADY_TAKEN,   ///< The tick has already been taken and simulated
  TOO_FAR_AHEAD,   ///< Beyond `INPUT_QUEUE_TICKS` of the next tick
  UNKNOWN_PLAYER,  ///< The player index is outside the session
};

/// §4.1 step 1: the queue a tick drains. Inputs arrive per player and per
/// tick, in any order; a tick is taken only when every player's input for
/// it has arrived.
///
/// That one rule is the lockstep stall condition (ADR-005). Solo play
/// submits for the next tick and takes it at once; a networked session
/// submits its own input `delay` ticks ahead and waits on its peers'.
///
/// Fixed storage: no allocation after construction.
class InputQueue {
public:
  /// A queue for `player_count` players (1 to `MAX_PLAYERS`) whose first
  /// tick is 0.
  explicit InputQueue(uint8_t player_count);

  /// Stores `input` as `player`'s input for `tick`.
  InputSubmitResult submit(uint8_t player, uint64_t tick,
                           const PlayerInput& input);

  /// The next tick's input when every player's has arrived, advancing to
  /// the tick after it; otherwise nothing, and the queue does not move.
  std::optional<TickInput> take();

  /// The tick `take` will return next.
  [[nodiscard]] uint64_t nextTick() const { return next_tick_; }

private:
  /// Inputs by `tick % INPUT_QUEUE_TICKS`.
  std::array<TickInput, INPUT_QUEUE_TICKS> inputs_{};
  /// Per ring slot, a bit for each player whose input has arrived.
  std::array<uint8_t, INPUT_QUEUE_TICKS> received_{};
  /// The value `received_` holds once every player has submitted.
  uint8_t complete_mask_ = 0;
  /// Players in the session.
  uint8_t player_count_ = 0;
  /// The tick `take` returns next.
  uint64_t next_tick_ = 0;
};

}  // namespace eng::sim
