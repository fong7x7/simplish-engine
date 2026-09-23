#pragma once

/// @file editor-footstep-surfaces.h
/// @brief What a level sounds like underfoot, from what the editor holds.
/// @par Threading Thread-safe (pure function over value types).

#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-document.h>
#include <game/fx/footstep-surfaces.h>
#include <vector>

namespace eng::editor {

/// The surfaces a playtest of @p document hears steps on: each painted
/// cell as its terrain's surface (`EditorTerrain::surface`), and over it
/// every prop with a surface of its own, as the box the viewport outlines
/// round it measured against @p assets. A prop whose asset the list no
/// longer has is left out.
[[nodiscard]] game::FootstepSurfaces
makeEditorFootstepSurfaces(const EditorDocument& document,
                           const std::vector<EditorAsset>& assets);

}  // namespace eng::editor
