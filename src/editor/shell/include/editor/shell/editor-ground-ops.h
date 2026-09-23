#pragma once

/// @file editor-ground-ops.h
/// @brief Painting the ground, and recording what a paint changed.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstdint>
#include <editor/shell/editor-ground-change.h>
#include <editor/shell/editor-ground-side.h>
#include <editor/shell/iso-projection.h>
#include <engine/render-ground/ground-grid.h>
#include <engine/render-ground/ground-rect.h>
#include <span>
#include <vector>

namespace eng::editor {

/// The smallest brush: one tile.
inline constexpr int32_t EDITOR_BRUSH_SIZE_MIN = 1;

/// The largest brush, in tiles on a side.
inline constexpr int32_t EDITOR_BRUSH_SIZE_MAX = 9;

/// How many tiles on a side, at most, one call to the agent's
/// `paint_ground` fills — a level's width, twice over.
inline constexpr int32_t EDITOR_GROUND_FILL_MAX = 256;

/// The square a brush @p size tiles across paints with its middle on the
/// tile under @p point. An even size has no middle tile, and reaches one
/// further south-west than north-east.
[[nodiscard]] GroundRect editorBrushRect(WorldPoint point, int32_t size);

/// Paint every cell of @p rect in @p grid with @p terrain. Cells past the
/// grid's coordinate limit are left alone. True when any cell changed.
bool paintEditorGround(GroundGrid& grid, GroundRect rect, uint8_t terrain);

/// Every cell that differs between @p before and @p after, in row order
/// from the south-west: what one edit that turned the first into the second
/// changed, and so the whole of what undoing it has to put back.
[[nodiscard]] std::vector<EditorGroundChange>
diffEditorGround(const GroundGrid& before, const GroundGrid& after);

/// Write @p side of every change in @p changes into @p grid.
void applyEditorGroundChanges(GroundGrid& grid,
                              std::span<const EditorGroundChange> changes,
                              EditorGroundSide side);

}  // namespace eng::editor
