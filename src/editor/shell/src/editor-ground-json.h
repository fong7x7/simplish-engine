#pragma once

/// @file editor-ground-json.h
/// @brief The ground's part of a level file.
/// @par Threading Thread-safe (pure functions over value types).

#include <engine/render-ground/ground-grid.h>
#include <engine/render-water/water-layer.h>
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

/// Write @p water into @p content as `layers.water`: its own `bounds`, and
/// runs of each of its grids over them — `depth` in steps of `step` tiles,
/// 0 for dry, then `red`, `green`, `blue` and `opacity` as bytes
/// (project-format.md §4.1). Nothing is written when there is no water.
/// Call after `writeEditorGround`, which writes `layers`.
void writeEditorWater(const WaterLayer& water, nlohmann::json& content);

/// The water a level's @p content describes; none when its layer cannot
/// be read. A level written before water was a layer of its own painted it
/// as a terrain: each such cell is read as water in the default colour at
/// the depth its old `water_depth` layer gave it, and given on @p ground
/// the sand that was drawn under it.
[[nodiscard]] WaterLayer readEditorWater(const nlohmann::json& content,
                                         GroundGrid& ground);

}  // namespace eng::editor
