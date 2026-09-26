#include "deployed-pacer.h"

#include <thread>

namespace eng::editor {

DeployedPacer::DeployedPacer(DeployedPace pace)
  : pace_(pace), last_(std::chrono::steady_clock::now()) {}

uint32_t DeployedPacer::due() {
  if (pace_ == DeployedPace::FAST) {
    return 1;
  }
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_);
  last_ = now;
  return clock_.advance(static_cast<uint64_t>(elapsed.count())).ticks;
}

void DeployedPacer::rest() const {
  if (pace_ == DeployedPace::REAL_TIME) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

}  // namespace eng::editor
