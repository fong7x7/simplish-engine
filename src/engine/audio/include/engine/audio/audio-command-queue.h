#pragma once

/// @file audio-command-queue.h
/// @brief Requests from one thread to another, without a lock.
/// @par Threading
/// One producer thread calls `push`, one consumer thread calls `pop`. Any
/// more of either is a race.

#include <atomic>
#include <cstdint>
#include <engine/audio/audio-command.h>
#include <optional>
#include <vector>

namespace eng::audio {

/// A fixed ring of commands between the main thread and the mixer's. The
/// device's thread must never wait on the main one — a wait there is a gap
/// in the sound — so neither side locks: each owns one index and only reads
/// the other's. A full ring refuses the command rather than growing.
class AudioCommandQueue {
public:
  /// A queue for at least @p capacity commands, rounded up to a power of
  /// two.
  explicit AudioCommandQueue(uint32_t capacity);

  /// Add @p command at the back; false when the ring is full.
  bool push(const AudioCommand& command);

  /// Take the command at the front, if there is one.
  std::optional<AudioCommand> pop();

  /// How many commands the ring holds at most.
  [[nodiscard]] uint32_t capacity() const;

private:
  /// The ring.
  std::vector<AudioCommand> slots_{};
  /// Capacity minus one, to wrap an index.
  uint32_t mask_ = 0;
  /// How many commands have been pushed; written by the producer only.
  std::atomic<uint32_t> head_{0};
  /// How many have been popped; written by the consumer only.
  std::atomic<uint32_t> tail_{0};
};

}  // namespace eng::audio
