#include <engine/core/assert.h>
#include <engine/sim/replay-recorder.h>
#include <utility>

namespace eng::sim {

namespace {

  /// Ten minutes of ticks at 60 Hz: the input reserved at construction.
  constexpr std::size_t RESERVED_TICKS = 60U * 60U * 10U;

}  // namespace

ReplayRecorder::ReplayRecorder(ReplayHeader header,
                               uint64_t checkpoint_interval)
  : checkpoint_interval_(checkpoint_interval) {
  // ENGINE_ASSERT expands to the do-while statement-macro idiom.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(checkpoint_interval > 0,
                "ReplayRecorder needs a checkpoint interval of at least 1");
  replay_.header = std::move(header);
  replay_.inputs.reserve(RESERVED_TICKS);
  replay_.checkpoints.reserve(RESERVED_TICKS / checkpoint_interval + 1U);
}

void ReplayRecorder::record(const TickInput& input, const TickResult& result) {
  // ENGINE_ASSERT expands to the do-while statement-macro idiom.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
  ENGINE_ASSERT(result.tick == replay_.inputs.size(),
                "ReplayRecorder::record got ticks out of order");
  replay_.inputs.push_back(input);
  if (!result.hash) {
    return;
  }
  last_hash_ = result.hash;
  if (result.tick % checkpoint_interval_ == 0) {
    replay_.checkpoints.push_back(*result.hash);
  }
}

Replay ReplayRecorder::finish() const {
  Replay replay = replay_;
  if (!last_hash_) {
    return replay;
  }
  if (replay.checkpoints.empty() ||
      replay.checkpoints.back().tick != last_hash_->tick) {
    replay.checkpoints.push_back(*last_hash_);
  }
  return replay;
}

}  // namespace eng::sim
