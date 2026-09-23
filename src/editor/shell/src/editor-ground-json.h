#pragma once

/// @file editor-ground-json.h
/// @brief The ground's part of a level file.
/// @par Threading Thread-safe (pure functions over value types).

#include <engine/render-ground/ground-grid.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// Write @p ground into a level's @p content: `bounds`, the cells the layer
/// covers; `tile_palette`, every terrain by reference, bare ground first;
/// and `layers.terrain`, the cells as runs of palette numbers in row order
/// from the south-west (project-format.md §4).
///
/// Nothing is written when nothing is painted, so a level saved before
/// there was a ground saves back unchanged.
void writeEditorGround(const GroundGrid& ground, nlohmann::json& content);

/// The ground a level's @p content describes, bare when it describes none
/// or describes it in a way that cannot be read — runs that do not cover
/// their bounds, or an encoding other than `rle`. A palette entry naming a
/// terrain the editor does not have reads as bare ground.
[[nodiscard]] GroundGrid readEditorGround(const nlohmann::json& content);

}  // namespace eng::editor
