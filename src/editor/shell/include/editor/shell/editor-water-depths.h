#pragma once

/// @file editor-water-depths.h
/// @brief The depths the editor paints water at, and how it names a depth.
/// @par Threading Thread-safe (immutable data and pure functions).

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-water-depth.h>
#include <optional>
#include <string>
#include <string_view>

namespace eng::editor {

/// Every named depth, shallowest first: from a puddle a boot splashes
/// through to water nobody sees the bottom of. Any depth in between can be
/// set by number (`set_water_depth`); these are what the brush steps
/// through and the panel lists.
inline constexpr EditorWaterDepth EDITOR_WATER_DEPTHS[] = {
    {"Puddle", "puddle", 1.0f / 16.0f},
    {"Shallows", "shallows", 0.25f},
    {"Pond", "pond", 1.0f},
    {"Lake", "lake", 3.0f},
    {"Deep", "deep", 8.0f},
};

/// How many named depths there are.
inline constexpr size_t EDITOR_WATER_DEPTH_COUNT =
    sizeof(EDITOR_WATER_DEPTHS) / sizeof(EDITOR_WATER_DEPTHS[0]);

/// The named depth new water is painted at: a pond, which is also what
/// water nobody gave a depth reads as (`WATER_DEFAULT_DEPTH`).
inline constexpr size_t EDITOR_DEFAULT_WATER_DEPTH = 2;

/// The depth @p word names, in tiles: a named depth's word, or a number of
/// tiles from a sixteenth to `WATER_MAX_DEPTH`. Nothing for anything else.
[[nodiscard]] std::optional<float> editorWaterDepthNamed(std::string_view word);

/// The named depth nearest @p tiles, reckoning by ratio rather than
/// difference: a quarter of a tile is nearer a puddle than a pond.
[[nodiscard]] size_t editorNearestWaterDepth(float tiles);

/// How a stored depth reads in the panel and the status line: its name
/// when it is exactly a named depth, then its tiles — `Pond (1 tile)`,
/// `1.75 tiles`.
[[nodiscard]] std::string editorWaterDepthLabel(uint8_t units);

}  // namespace eng::editor
