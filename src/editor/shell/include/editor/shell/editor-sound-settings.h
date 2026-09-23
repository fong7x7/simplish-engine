#pragma once

/// @file editor-sound-settings.h
/// @brief The user's volume settings, as the shell holds them.
/// @par Threading Main-thread only.

#include <cstdint>
#include <engine/audio/audio-volumes.h>
#include <filesystem>

namespace eng::editor {

/// How loud the user wants the editor's sound, where that is kept, and a
/// count of changes, so whatever changes it — the Sound screen, an agent's
/// `set_volume` — only has to bump `revision` for the editor to apply and
/// save it.
struct EditorSoundSettings {
  /// Master, each bus, and the mute.
  audio::AudioVolumes volumes{};
  /// The file they are kept in; empty when there is nowhere to keep them.
  std::filesystem::path file;
  /// Bumped on every change; the editor applies and saves when it moves on
  /// from the revision it last wrote.
  uint64_t revision = 0;
};

}  // namespace eng::editor
