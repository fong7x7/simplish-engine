#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <engine/core/assert.h>
#include <engine/core/frame-timings-config.h>
#include <engine/core/frame-timings-history.h>
#include <engine/core/frame-timings.h>
#include <engine/core/logger.h>
#include <engine/core/scoped-frame-timer.h>
#include <engine/core/thread-context.h>
#include <string_view>
#include <vector>

namespace eng {

namespace {

  /// Clamp the configured capacity to the permitted minimum, logging a warning
  /// if the raw value was invalid.
  std::size_t clampCapacity(std::size_t raw) {
    if (raw < MIN_FRAME_TIMINGS_RING_CAPACITY) {
      Logger::warn("FrameTimingsHistory",
                   "ring_capacity below minimum; clamping up");
      return MIN_FRAME_TIMINGS_RING_CAPACITY;
    }
    return raw;
  }

#if !(defined(ENGINE_FRAME_TIMING) && ENGINE_FRAME_TIMING == 0)
  /// If a ThreadContext is registered for this thread, assert main. When no
  /// context is registered (e.g. unit tests running without init), skip the
  /// check — consistent with how other core utilities behave pre-init.
  void assertMainIfRegistered(std::string_view op) {
    auto* ctx = ThreadContext::get();
    if (ctx != nullptr) {
      ctx->assertMainThread(op);
    }
  }
#endif

}  // namespace

FrameTimingsHistory::FrameTimingsHistory(const FrameTimingsConfig& config)
  : capacity_(clampCapacity(config.ring_capacity)), enabled_(config.enabled) {
  ring_.resize(capacity_);
}

FrameTimings& FrameTimingsHistory::beginFrame(uint64_t frame_number) {
#if defined(ENGINE_FRAME_TIMING) && ENGINE_FRAME_TIMING == 0
  (void)frame_number;
  return disabled_slot_;
#else
  assertMainIfRegistered("FrameTimingsHistory::beginFrame");
  if (!enabled_) {
    return disabled_slot_;
  }
  ENGINE_ASSERT(!in_frame_, "beginFrame called without matching endFrame");
  scratch_ = FrameTimings{};
  scratch_.frame_number = frame_number;
  scratch_.frame_start_ns = currentSteadyNanoseconds();
  in_frame_ = true;
  return scratch_;
#endif
}

void FrameTimingsHistory::endFrame() {
#if defined(ENGINE_FRAME_TIMING) && ENGINE_FRAME_TIMING == 0
  return;
#else
  assertMainIfRegistered("FrameTimingsHistory::endFrame");
  if (!enabled_) {
    return;
  }
  ENGINE_ASSERT(in_frame_, "endFrame called without matching beginFrame");
  scratch_.frame_duration_ns =
      currentSteadyNanoseconds() - scratch_.frame_start_ns;
  ring_[write_index_] = scratch_;
  write_index_ = (write_index_ + 1) % capacity_;
  count_ = std::min(count_ + 1, capacity_);
  in_frame_ = false;
#endif
}

std::vector<FrameTimings> FrameTimingsHistory::snapshot() const {
  std::vector<FrameTimings> out;
  out.reserve(count_);
  const std::size_t start =
      (count_ < capacity_) ? 0 : write_index_;  // oldest committed slot
  for (std::size_t i = 0; i < count_; ++i) {
    out.push_back(ring_[(start + i) % capacity_]);
  }
  return out;
}

std::size_t FrameTimingsHistory::capacity() const {
  return capacity_;
}

std::size_t FrameTimingsHistory::size() const {
  return count_;
}

bool FrameTimingsHistory::enabled() const {
  return enabled_;
}

}  // namespace eng
