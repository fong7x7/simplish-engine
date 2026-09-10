#include <engine/core/assert.h>
#include <engine/sim/input-queue.h>

namespace eng::sim {

namespace {

  /// The ring slot holding `tick`.
  std::size_t ringSlot(uint64_t tick) {
    return static_cast<std::size_t>(tick % INPUT_QUEUE_TICKS);
  }

  /// Why `tick` cannot be queued while `next_tick` is next, or nothing when
  /// it can.
  std::optional<InputSubmitResult> tickOutOfRange(uint64_t tick,
                                                  uint64_t next_tick) {
    if (tick < next_tick) {
      return InputSubmitResult::ALREADY_TAKEN;
    }
    if (tick - next_tick >= INPUT_QUEUE_TICKS) {
      return InputSubmitResult::TOO_FAR_AHEAD;
    }
    return std::nullopt;
  }

}  // namespace

InputQueue::InputQueue(uint8_t player_count)
  : complete_mask_(static_cast<uint8_t>((1U << player_count) - 1U)),
    player_count_(player_count) {
  // ENGINE_ASSERT is a do-while macro that negates its whole condition.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while,readability-simplify-boolean-expr)
  ENGINE_ASSERT(player_count >= 1 && player_count <= MAX_PLAYERS,
                "InputQueue needs between 1 and MAX_PLAYERS players");
}

InputSubmitResult InputQueue::submit(uint8_t player, uint64_t tick,
                                     const PlayerInput& input) {
  if (player >= player_count_) {
    return InputSubmitResult::UNKNOWN_PLAYER;
  }
  if (const auto rejected = tickOutOfRange(tick, next_tick_)) {
    return *rejected;
  }
  const std::size_t slot = ringSlot(tick);
  const auto bit = static_cast<uint8_t>(1U << player);
  if ((received_[slot] & bit) != 0) {
    return InputSubmitResult::DUPLICATE;
  }
  inputs_[slot].players[player] = input;
  received_[slot] = static_cast<uint8_t>(received_[slot] | bit);
  return InputSubmitResult::ACCEPTED;
}

std::optional<TickInput> InputQueue::take() {
  const std::size_t slot = ringSlot(next_tick_);
  if (received_[slot] != complete_mask_) {
    return std::nullopt;
  }
  const TickInput input = inputs_[slot];
  inputs_[slot] = TickInput{};
  received_[slot] = 0;
  ++next_tick_;
  return input;
}

}  // namespace eng::sim
