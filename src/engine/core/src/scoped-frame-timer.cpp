#include <chrono>
#include <cstdint>
#include <engine/core/scoped-frame-timer.h>

namespace eng {

uint64_t currentSteadyNanoseconds() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

#if defined(ENGINE_FRAME_TIMING) && ENGINE_FRAME_TIMING == 0

ScopedFrameTimer::ScopedFrameTimer(PhaseTimings& /*target*/) {}

ScopedFrameTimer::~ScopedFrameTimer() = default;

#else

ScopedFrameTimer::ScopedFrameTimer(PhaseTimings& target)
  : target_(&target), start_ns_(currentSteadyNanoseconds()) {
  target_->start_ns = start_ns_;
  target_->duration_ns = 0;
}

ScopedFrameTimer::~ScopedFrameTimer() {
  target_->duration_ns = currentSteadyNanoseconds() - start_ns_;
}

#endif

}  // namespace eng
