#pragma once

/// @file editor-deploy-manifest.h
/// @brief What a deployed game holds, and which level it starts on.
/// @par Threading Thread-safe (value type and pure functions).

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// Schema tag of a deployed game's manifest.
inline constexpr std::string_view EDITOR_DEPLOY_SCHEMA = "simplish/deploy/1.0";

/// The file, inside a deployed game's content, listing what it holds.
inline constexpr std::string_view EDITOR_DEPLOY_MANIFEST = "manifest.json";

/// The folder, inside a deployed game's content, holding one baked setup
/// per level.
inline constexpr std::string_view EDITOR_DEPLOY_LEVELS_DIR = "levels";

/// The extension of a baked setup: `<level>.setup.json`.
inline constexpr std::string_view EDITOR_SETUP_FILE_SUFFIX = ".setup.json";

/// A deployed game's table of contents, written beside its content.
/// @thread_safety Immutable value type.
struct EditorDeployManifest {
  /// The project's name.
  std::string name;
  /// Every level baked, by id, in the order the project lists them.
  std::vector<std::string> levels;
  /// The level the game starts on.
  std::string start_level;
  /// Whether the project's own game logic is linked in.
  bool has_logic = false;
  /// `projectLogicHash` of the logic linked in; 0 for none. Part of the
  /// content, so a game deployed from other logic is other content.
  uint64_t logic_hash = 0;
};

/// @p manifest as JSON.
[[nodiscard]] std::string
serializeDeployManifest(const EditorDeployManifest& manifest);

/// A manifest read back from @p text, or nothing when it is not one.
[[nodiscard]] std::optional<EditorDeployManifest>
parseDeployManifest(std::string_view text);

}  // namespace eng::editor
