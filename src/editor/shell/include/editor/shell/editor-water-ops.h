#pragma once

/// @file editor-water-ops.h
/// @brief Painting, drying, deepening and colouring the water layer, as
/// undoable edits.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstdint>
#include <editor/shell/editor-action.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-ground-side.h>
#include <editor/shell/editor-property-field.h>
#include <editor/shell/editor-water-change.h>
#include <engine/render-ground/ground-cell.h>
#include <engine/render-ground/ground-rect.h>
#include <engine/render-water/water-layer.h>
#include <optional>
#include <span>
#include <vector>

namespace eng::editor {

/// The rows the properties panel lists for a body of water, below its
/// Depth row: its colour and how opaque it is, each a slider, then which
/// way it flows, in degrees, and how fast, a slider, and how thick it is,
/// a slider.
inline constexpr EditorPropertyField EDITOR_WATER_FIELDS[] = {
    EditorPropertyField::COLOR_R,        EditorPropertyField::COLOR_G,
    EditorPropertyField::COLOR_B,        EditorPropertyField::OPACITY,
    EditorPropertyField::FLOW_DIRECTION, EditorPropertyField::FLOW_SPEED,
    EditorPropertyField::VISCOSITY};

/// Lay water over every cell of @p rect: @p water's depth everywhere, and
/// on cells that were dry its colour and opacity too. Water already there
/// keeps its own colour and opacity, so a brush can deepen part of a lake
/// without repainting it. Returns whether anything changed.
bool layEditorWater(WaterLayer& layer, GroundRect rect, const WaterCell& water);

/// Take the water off every cell of @p rect. Returns whether any was there.
bool dryEditorWater(WaterLayer& layer, GroundRect rect);

/// Give every cell of @p cells that holds water a depth of @p units; dry
/// cells are left dry. Returns whether any depth changed.
bool setEditorWaterDepth(WaterLayer& layer, std::span<const GroundCell> cells,
                         uint8_t units);

/// The same over every cell of @p rect.
bool setEditorWaterDepthIn(WaterLayer& layer, GroundRect rect, uint8_t units);

/// Set @p field — a colour channel, the opacity, the flow's speed or the
/// viscosity, 0 to 1, or the flow's direction in degrees — of every cell of @p
/// cells that holds water. Returns whether any changed.
bool setEditorWaterValue(WaterLayer& layer, std::span<const GroundCell> cells,
                         EditorPropertyField field, float value);

/// @p value of @p field — as `setEditorWaterValue` takes it — as the byte
/// a `WaterCell` stores.
[[nodiscard]] uint8_t editorWaterByte(EditorPropertyField field, float value);

/// @p field of @p water as the panel shows it: 0 to 1, or degrees from
/// −180 to 180 for the flow's direction.
[[nodiscard]] float editorWaterValue(const WaterCell& water,
                                     EditorPropertyField field);

/// Every body of water @p cell is in: every cell of water joined to it,
/// corner to corner included, whatever their depths or colours. Empty when
/// @p cell is dry.
[[nodiscard]] std::vector<GroundCell>
connectedWaterCells(const WaterLayer& layer, GroundCell cell);

/// Every cell whose water differs between @p before and @p after, with
/// both sides.
[[nodiscard]] std::vector<EditorWaterChange>
diffEditorWater(const WaterLayer& before, const WaterLayer& after);

/// Write @p side of every change back into @p layer.
void applyEditorWaterChanges(WaterLayer& layer,
                             std::span<const EditorWaterChange> changes,
                             EditorGroundSide side);

/// The edit that makes @p document's water @p after, or nothing when it
/// already is.
[[nodiscard]] std::optional<EditorAction>
editorWaterEdit(const EditorDocument& document, const WaterLayer& after);

}  // namespace eng::editor
