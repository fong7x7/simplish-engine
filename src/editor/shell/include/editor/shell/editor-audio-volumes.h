#pragma once

/// @file editor-audio-volumes.h
/// @brief The user's volume settings, kept in their application data.
/// @par Threading Main-thread only (reads and writes a file).

#include <engine/audio/audio-volumes.h>
#include <filesystem>

namespace eng::editor {

/// The volume settings in @p file, over full volume.
///
/// The file is the user's, not the project's, as their controls are: a
/// quieter editor stays quieter in every project. When it does not exist
/// yet the defaults are written to it, so there is a complete file to edit.
/// Entries it gets wrong are logged and skipped. An empty @p file keeps
/// nothing and reads nothing.
[[nodiscard]] audio::AudioVolumes
loadEditorAudioVolumes(const std::filesystem::path& file);

/// Write @p volumes to @p file, creating its directory. False, and logged,
/// when it could not; nothing is written for an empty @p file.
bool saveEditorAudioVolumes(const std::filesystem::path& file,
                            const audio::AudioVolumes& volumes);

}  // namespace eng::editor
