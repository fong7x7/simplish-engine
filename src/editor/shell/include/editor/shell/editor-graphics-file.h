#pragma once

/// @file editor-graphics-file.h
/// @brief The user's graphics settings, kept in their application data.
/// @par Threading Main-thread only (reads and writes a file).

#include <editor/shell/editor-graphics-settings.h>
#include <filesystem>
#include <string>
#include <vector>

namespace eng::editor {

/// The graphics settings @p text holds, over the defaults, with a line
/// for every entry it gets wrong in @p problems. The file is one object:
/// `{"water": "high", "water_effects": {"reflections": true, ...}}`, every
/// effect `WATER_EFFECT_WORDS` names a boolean, and one left out on.
[[nodiscard]] EditorGraphicsSettings
parseEditorGraphics(const std::string& text,
                    std::vector<std::string>& problems);

/// @p settings as the file holds them.
[[nodiscard]] std::string
writeEditorGraphics(const EditorGraphicsSettings& settings);

/// The graphics settings in @p file, over the defaults, remembering the
/// file. When it does not exist yet the defaults are written to it, so
/// there is a complete file to edit. Entries it gets wrong are logged and
/// skipped. An empty @p file keeps nothing and reads nothing.
[[nodiscard]] EditorGraphicsSettings
loadEditorGraphics(const std::filesystem::path& file);

/// Write @p settings to their file, creating its directory. False, and
/// logged, when it could not; nothing is written when they have no file.
bool saveEditorGraphics(const EditorGraphicsSettings& settings);

}  // namespace eng::editor
