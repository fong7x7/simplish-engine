#pragma once

// Design Summary -- Tempest Audio Backend
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/tempest-audio-backend.md
//
// Behaviours:
//   - Implement IAudioBackend using PS5 Tempest 3D Audio Engine
//   - Map play3d() to sceAudio3dObjectSetAttributes() for hardware 3D audio
//   - Support up to 128 simultaneous hardware audio sources
//   - Use personalised HRTF from PS5 user profile when available
//   - Decode ATRAC9 via hardware decoder; fall back to OGG Vorbis
//   - Stream audio on background thread into SceAudioOut ring buffers
//   - Identical C++ API to desktop audio backend
//
// Edge Cases:
//   - Tempest init failure: create() returns nullptr
//   - All 128 sources in use: steal lowest-priority source
//   - Personalised HRTF unavailable: use generic HRTF from SDK
//   - ATRAC9 asset missing: fall back to OGG Vorbis version
//   - Invalid SourceId: assert in debug, ignore in shipping
//   - Streaming decode error: skip frame, continue playback
//
// Invariants:
//   - IAudioBackend is the sole path to audio APIs
//   - Tempest SDK headers never included in this public header
//   - Backend not accessed from multiple threads (main thread only; streaming
//   is internal)
//   - Source priority determines steal order
//
// Integration Points:
//   - IAudioBackend: TempestAudioBackend is PS5 concrete implementation
//   - AudioSystemContext: owns via unique_ptr<IAudioBackend>
//   - CMake: ENGINE_AUDIO_BACKEND=TEMPEST selects this implementation

#include "ps5-audio-config.h"
#include "ps5-types.h"

#include <cstdint>
#include <memory>

namespace eng {

// Forward declaration
class IAudioBackend;

// ---------------------------------------------------------------------------
// Tempest Audio Backend
// ---------------------------------------------------------------------------

/// PS5 Tempest 3D Audio implementation of IAudioBackend.
/// Created via static factory; returns nullptr on Tempest init failure.
/// All public methods are main thread only.
///
/// Internal Tempest state (SceAudio3d context, object handles, ring
/// buffers) is managed in the .cpp file and not exposed here.
class TempestAudioBackend {
public:
  /// Create and initialise the Tempest backend. Returns nullptr if
  /// Tempest 3D Audio initialisation fails.
  /// Main thread only.
  static std::unique_ptr<TempestAudioBackend>
  create(const Ps5AudioConfig& config);

  ~TempestAudioBackend();

  TempestAudioBackend(TempestAudioBackend&& other) noexcept;
  TempestAudioBackend& operator=(TempestAudioBackend&& other) noexcept;

  TempestAudioBackend(const TempestAudioBackend&) = delete;
  TempestAudioBackend& operator=(const TempestAudioBackend&) = delete;

  /// Returns the number of currently active audio sources.
  uint32_t activeSourceCount() const;

  /// Returns the codec preference in use.
  Ps5AudioCodecPreference codecPreference() const;

private:
  TempestAudioBackend();

  struct Impl;
  /// Opaque implementation holding Tempest 3D Audio state.
  std::unique_ptr<Impl> impl_;
};

}  // namespace eng
