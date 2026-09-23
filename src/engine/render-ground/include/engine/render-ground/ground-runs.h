#pragma once

/// @file ground-runs.h
/// @brief A ground grid as runs, and back.
/// @par Threading Thread-safe (pure functions over value types).

#include <engine/render-ground/ground-grid.h>
#include <engine/render-ground/ground-rect.h>
#include <engine/render-ground/ground-run.h>
#include <optional>
#include <span>
#include <vector>

namespace eng {

/// The cells of @p grid inside @p bounds, as runs in row order from the
/// south-west. Cells of @p bounds the grid does not hold are bare.
[[nodiscard]] std::vector<GroundRun> encodeGroundRuns(const GroundGrid& grid,
                                                      GroundRect bounds);

/// The grid @p runs describe over @p bounds. Nothing when the runs do not
/// cover the rectangle exactly — too few cells, or too many — since a grid
/// guessed from a truncated file would silently shift every row after the
/// break.
[[nodiscard]] std::optional<GroundGrid>
decodeGroundRuns(GroundRect bounds, std::span<const GroundRun> runs);

}  // namespace eng
