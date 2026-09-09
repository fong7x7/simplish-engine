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

/// The id of the one level a project has today.
///
/// The editor authors a single level per project, so its id is a constant
/// rather than something to choose. A level browser is what turns this into
/// a choice, and until there is one, a name here would be a setting with no
/// interface to change it.
inline constexpr std::string_view EDITOR_LEVEL_ID = "main";

/// Serialise what has been authored as a level file, named @p name.
///
/// Props reference their asset by the same `kind:id` string every other
/// file in the project format uses (`mesh:props_crate`, `shape:cube`),
/// never by the index the session happens to hold it at: the index is a
/// handle that a rescan renumbers, and the id is the thing that survives
/// one. Arrays are written in document order, because that order is what
/// the runtime iterates ([project-format.md §3]).
[[nodiscard]] std::string
serializeEditorLevel(const EditorDocument& document,
                     const std::vector<EditorAsset>& assets,
                     std::string_view name);

/// Read a level file back, binding each prop to its asset in @p assets.
///
/// Nothing when @p text is not valid JSON, is not an object, or names a
/// schema this build does not write. A prop whose asset the list does not
/// hold is dropped and counted rather than failing the whole read: one
/// deleted `.obj` should not cost a level everything else in it.
[[nodiscard]] std::optional<EditorLevelLoad>
parseEditorLevel(std::string_view text, const std::vector<EditorAsset>& assets);

}  // namespace eng::editor
