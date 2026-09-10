#pragma once

/// @file editor-level-json.h
/// @brief The level file, as text: what the editor writes and reads back.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-level-load.h>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// Schema id every level file this build writes carries, and the only one
/// it reads. A file naming another version is refused rather than partly
/// read — see [project-format.md §10].
inline constexpr std::string_view EDITOR_LEVEL_SCHEMA = "simplish/level/1.0";

/// The id of the level a project starts at.
///
/// A project holds as many levels as it is given, and the Level menu
/// chooses between them; this is the one a new project is created with and
/// the one an opened project falls back to when it holds no level file at
/// all. Every other id is authored.
inline constexpr std::string_view EDITOR_LEVEL_ID = "main";

/// Serialise what has been authored as the level @p id's file.
///
/// The file's `name` is the id, because nothing in the editor authors a
/// display name for a level yet. Writing the project's name there instead
/// would put the same name in every level file of a project holding
/// several, which is a worse answer than the id.
///
/// Props reference their asset by the same `kind:id` string every other
/// file in the project format uses (`mesh:props_crate`, `shape:cube`),
/// never by the index the session happens to hold it at: the index is a
/// handle that a rescan renumbers, and the id is the thing that survives
/// one. Arrays are written in document order, because that order is what
/// the runtime iterates ([project-format.md §3]). Player starts go in
/// `entities`, as the format's `entity:player_start` definition.
[[nodiscard]] std::string
serializeEditorLevel(const EditorDocument& document,
                     const std::vector<EditorAsset>& assets,
                     std::string_view id);

/// Read a level file back, binding each prop to its asset in @p assets.
///
/// Nothing when @p text is not valid JSON, is not an object, or names a
/// schema this build does not write. A prop whose asset the list does not
/// hold is dropped and counted rather than failing the whole read: one
/// deleted `.obj` should not cost a level everything else in it. An entity
/// whose definition the editor does not know is dropped and counted the
/// same way.
[[nodiscard]] std::optional<EditorLevelLoad>
parseEditorLevel(std::string_view text, const std::vector<EditorAsset>& assets);

}  // namespace eng::editor
