#pragma once

/// @file audio-command-kind.h
/// @brief What the main thread can ask the mixer to do.
/// @par Threading
/// A value type.

#include <cstdint>

namespace eng::audio {

/// Which request an `AudioCommand` carries.
enum class AudioCommandKind : uint8_t {
  /// Start `start`.
  START,
  /// Stop the sound `start.sound` names.
  STOP,
  /// Stop every sound.
  STOP_ALL,
  /// Set `bus`'s volume to `gain`.
  BUS_GAIN,
  /// Set the master volume to `gain`.
  MASTER_GAIN,
  /// Move the ears to `listener`.
  LISTENER,
};

/// How many kinds of command there are.
inline constexpr uint8_t AUDIO_COMMAND_KIND_COUNT = 6;

}  // namespace eng::audio
