#pragma once

/// @file audio-command.h
/// @brief One request from the main thread to the mixer's.
/// @par Threading
/// A value type, copied through `AudioCommandQueue`.

#include <engine/audio/audio-bus.h>
#include <engine/audio/audio-command-kind.h>
#include <engine/audio/audio-listener.h>
#include <engine/audio/voice-start.h>

namespace eng::audio {

/// A request for the mixer, as it crosses between threads: a kind, and the
/// fields that kind reads. The rest are left as they default.
struct AudioCommand {
  /// What is asked.
  AudioCommandKind kind = AudioCommandKind::START;
  /// The sound to start, or — for STOP — the one to stop.
  VoiceStart start{};
  /// The bus whose volume is set.
  AudioBus bus = AudioBus::EFFECTS;
  /// The volume a bus or the master is set to.
  float gain = 1.0F;
  /// Where the ears move to.
  AudioListener listener{};
};

}  // namespace eng::audio
