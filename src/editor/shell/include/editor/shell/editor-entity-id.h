#pragma once

/// @file editor-entity-id.h
/// @brief Making the stable ids that name assets and what is placed.
/// @par Threading Main-thread-only (reads the document it mints against).

#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-id-kind.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// What an id becomes when nothing usable can be made of the text it came
/// from — a file named only in punctuation, say.
inline constexpr std::string_view EDITOR_ID_FALLBACK = "unnamed";

/// Turn arbitrary text into an id the project format will accept.
///
/// Ids are lowercase snake_case and have to be valid C++ identifiers,
/// because the content generator emits them as the names of generated
/// symbols ([project-format.md §3]). Anything that is not a letter or a
/// digit becomes an underscore, runs of underscores collapse, and the ends
/// are trimmed; an id that would start with a digit is prefixed, since a
/// C++ identifier may not.
[[nodiscard]] std::string makeEditorIdentifier(std::string_view text);

/// A reference to @p id, as another file would write it: `mesh:crate`.
[[nodiscard]] std::string editorQualifiedId(EditorIdKind kind,
                                            std::string_view id);

/// The id an asset at @p relative_path takes, extension dropped:
/// `props/crate.obj` becomes `props_crate`.
///
/// Derived from the asset's own path and nothing else, which is what makes
/// it stable. An id that depended on what else the scan found — a numeric
/// suffix handed out in scan order — would change the moment an unrelated
/// file was added beside it, and every level referencing the old one would
/// be wrong.
[[nodiscard]] std::string
editorAssetIdFromPath(const std::filesystem::path& relative_path);

/// Give every asset in @p assets its id.
///
/// Scanned models are named for their path and built-in shapes for
/// themselves. Two different paths can still slugify the same — `my-crate`
/// and `my_crate` — and the later of them takes a numbered suffix; that is
/// the one case where an id depends on scan order, and it is a case a
/// project can avoid by not naming two files the same word twice.
void assignEditorAssetIds(std::vector<EditorAsset>& assets);

/// How another file references @p asset: `mesh:props_crate` for a model on
/// disk, `shape:cube` for a built-in one.
[[nodiscard]] std::string editorAssetRef(const EditorAsset& asset);

/// How another file references the placement @p placement: `prop:crate_01`.
[[nodiscard]] std::string editorPlacementRef(const EditorPlacement& placement);

/// How another file references the light @p light: `light:point_01`.
[[nodiscard]] std::string editorLightRef(const EditorLight& light);

/// Point every placement in @p document at its asset's index in @p assets,
/// given the ids those indices meant before. Returns how many placements
/// were dropped, which is zero for the ordinary case of a file added.
///
/// This is what an id is for. A rescan renumbers the asset list, and
/// before ids there was nothing a placement could be rebound through, so
/// adding one file to a project threw away everything placed in it. A
/// placement whose asset the new list does not hold is dropped: there is no
/// mesh to draw for it and no id to name it by.
size_t rebindPlacementAssets(EditorDocument& document,
                             const std::vector<std::string>& previous_ids,
                             const std::vector<EditorAsset>& assets);

/// An id for a new placement of @p asset, free in @p document.
///
/// Numbered from the asset's own id, so the third crate placed is
/// `crate_03` and reads as one in a logic file. The number is the lowest
/// one free rather than a running count: undo removes a placement and frees
/// its id, and redo restores that same placement with the id it already
/// had, so nothing is handed out twice.
[[nodiscard]] std::string mintEditorPlacementId(const EditorDocument& document,
                                                const EditorAsset& asset);

/// An id for a new light of @p kind, free in @p document.
[[nodiscard]] std::string mintEditorLightId(const EditorDocument& document,
                                            EditorLightKind kind);

}  // namespace eng::editor
