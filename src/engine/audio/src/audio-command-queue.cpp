#include <algorithm>
#include <bit>
#include <engine/audio/audio-command-queue.h>

namespace eng::audio {

AudioCommandQueue::AudioCommandQueue(uint32_t capacity)
  : slots_(std::bit_ceil(std::max(capacity, 2U))),
    mask_(static_cast<uint32_t>(slots_.size() - 1)) {}

bool AudioCommandQueue::push(const AudioCommand& command) {
  const uint32_t head = head_.load(std::memory_order_relaxed);
  const uint32_t tail = tail_.load(std::memory_order_acquire);
  if (head - tail > mask_) {
    return false;
  }
  slots_[head & mask_] = command;
  head_.store(head + 1, std::memory_order_release);
  return true;
}

std::optional<AudioCommand> AudioCommandQueue::pop() {
  const uint32_t tail = tail_.load(std::memory_order_relaxed);
  const uint32_t head = head_.load(std::memory_order_acquire);
  if (tail == head) {
    return std::nullopt;
  }
  const AudioCommand command = slots_[tail & mask_];
  tail_.store(tail + 1, std::memory_order_release);
  return command;
}

uint32_t AudioCommandQueue::capacity() const {
  return static_cast<uint32_t>(slots_.size());
}

}  // namespace eng::audio
