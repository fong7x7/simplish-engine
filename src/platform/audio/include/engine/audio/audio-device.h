#pragma once

/// @file audio-device.h
/// @brief The speakers this platform has, fed from an `AudioEngine`.
/// @par Threading
/// Main-thread-only. While open, the backend calls the engine's `render`
/// from a thread of its own.

#include <engine/audio/audio-engine.h>
#include <optional>
#include <string>

namespace eng::audio {

/// The platform's audio backend: opens the default output at the engine's
/// rate, in interleaved stereo float, and pulls the mix from
/// `AudioEngine::render` whenever the hardware wants more.
///
/// Which backend exists is decided when the platform is built: exactly one
/// is compiled in (`src/platform/audio/CMakeLists.txt`). The desktop's is
/// SDL3's audio stream, whose callback runs on SDL's own audio thread; a
/// console's is its own; a target with none compiled in has no sound, and
/// says so from `open`. Nothing above this layer knows which it got.
class AudioDevice {
public:
  AudioDevice() = default;
  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;
  AudioDevice(AudioDevice&&) = delete;
  AudioDevice& operator=(AudioDevice&&) = delete;

  /// Open the default output and start pulling from @p engine, which must
  /// outlive the device or its `close`. A reason on failure, after which
  /// the game is simply silent — never fatal. Nothing if already open.
  std::optional<std::string> open(AudioEngine& engine);

  /// Stop pulling and close the output. When this returns, the backend
  /// will not call `render` again. Safe to call twice.
  void close();

  /// Whether `open` succeeded and `close` has not run since.
  [[nodiscard]] bool isOpen() const { return stream_ != nullptr; }

private:
  /// The backend's handle on the open output, kept opaque here so no SDL
  /// or console type crosses this header; null while closed.
  void* stream_ = nullptr;
};

}  // namespace eng::audio
