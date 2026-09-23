#pragma once

/// @file audio-volumes-load.h
/// @brief What reading a volume settings file found.
/// @par Threading
/// A value type.

#include <engine/audio/audio-volumes.h>
#include <string>
#include <vector>

namespace eng::audio {

/// What `parseAudioVolumes` read: the settings, and a sentence for each
/// part of the file it had to skip. A file with problems still loads — one
/// mistyped number should not reset every other volume.
struct AudioVolumesLoad {
  /// The settings the file describes, with defaults where it is silent.
  AudioVolumes volumes;
  /// One line per entry skipped, for the log.
  std::vector<std::string> problems;
};

}  // namespace eng::audio
