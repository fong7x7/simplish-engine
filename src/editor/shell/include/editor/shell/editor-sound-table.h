#pragma once

/// @file editor-sound-table.h
/// @brief The project's own sounds, read from and written to their table.
/// @par Threading Main-thread-only (touches the filesystem).

#include <cstdint>
#include <editor/shell/editor-sound-entry.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The schema every row of the sounds table declares.
inline constexpr std::string_view EDITOR_SOUND_ENTRY_SCHEMA =
    "simplish/sound/1.0";

/// The sounds table: which of the game's sound slots the project plays its
/// own file in, and a count of changes. Unlike the other tables, the editor
/// writes this one — the Sound screen and an agent's `set_sound` change it,
/// bump `revision`, and the editor saves it and reloads the clips.
/// @thread_safety Main-thread-only.
struct EditorSoundTable {
  /// Every slot the project gives a file, in file order; a slot not here
  /// plays its built-in sound.
  std::vector<EditorSoundEntry> sounds{};
  /// What was wrong with the file, or with a file it names, one line each,
  /// for the log and the agent API.
  std::vector<std::string> problems{};
  /// Bumped on every change; the editor saves when it moves on from the
  /// revision it last wrote.
  uint64_t revision = 0;
};

/// Where a project keeps its sounds: `<root>/content/data/sounds.data.json`.
[[nodiscard]] std::filesystem::path
editorSoundTablePath(const std::filesystem::path& root);

/// The sounds in the data table @p text (project-format §8.4). A row with no
/// slot, a slot an earlier row took, or no file is skipped and said in
/// `problems`; a file that is not a sounds table gives no sounds and one
/// problem. Whether a slot or a file exists is not checked here.
[[nodiscard]] EditorSoundTable parseEditorSoundTable(std::string_view text);

/// The sounds table under @p root. No sounds and no problems when the
/// project has none, which is where every project starts.
[[nodiscard]] EditorSoundTable
loadEditorSoundTable(const std::filesystem::path& root);

/// @p table as the data table `parseEditorSoundTable` reads.
[[nodiscard]] std::string writeEditorSoundTable(const EditorSoundTable& table);

/// Write @p table under @p root. False when it could not be written.
bool saveEditorSoundTable(const std::filesystem::path& root,
                          const EditorSoundTable& table);

}  // namespace eng::editor
