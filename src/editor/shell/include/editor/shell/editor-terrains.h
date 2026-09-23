#pragma once

/// @file editor-terrains.h
/// @brief The terrains the editor paints the ground with.
/// @par Threading Thread-safe (immutable data and pure functions).

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-terrain.h>
#include <optional>
#include <string_view>

namespace eng::editor {

/// Every terrain, in the order they stack: each is drawn over those before
/// it where they meet, so a road laid across sand runs over the sand and a
/// hole cuts through everything. A terrain's number in the ground grid is
/// its place here plus one, since 0 is bare ground.
inline constexpr EditorTerrain EDITOR_TERRAINS[] = {
    {"Grass", "grass", 86, 136, 62, 14},   {"Dirt", "dirt", 118, 88, 60, 12},
    {"Sand", "sand", 212, 188, 128, 10},   {"Water", "water", 56, 108, 160, 6},
    {"Stone", "stone", 138, 138, 134, 12}, {"Road", "road", 60, 60, 66, 5},
    {"Hole", "hole", 16, 14, 18, 3},
};

/// How many terrains there are, which is also the highest terrain number.
inline constexpr size_t EDITOR_TERRAIN_COUNT =
    sizeof(EDITOR_TERRAINS) / sizeof(EDITOR_TERRAINS[0]);

/// The word bare ground is named by: `tile:none` in a file, `none` to the
/// agent API. Painting it is erasing.
inline constexpr std::string_view EDITOR_BARE_GROUND_WORD = "none";

/// The name the eraser's card carries.
inline constexpr std::string_view EDITOR_ERASER_NAME = "Erase";

/// The prefix a level file's tile palette names a terrain with.
inline constexpr std::string_view EDITOR_TILE_REF_PREFIX = "tile:";

/// How many cards the browser's ground folder holds: one per terrain, then
/// the eraser.
inline constexpr size_t EDITOR_GROUND_CARD_COUNT = EDITOR_TERRAIN_COUNT + 1;

/// The terrain number card @p card of the ground folder paints with: its
/// terrain for the first `EDITOR_TERRAIN_COUNT`, bare ground for the
/// eraser after them.
[[nodiscard]] constexpr uint8_t editorGroundCardTerrain(size_t card) {
  return card < EDITOR_TERRAIN_COUNT ? static_cast<uint8_t>(card + 1) : 0;
}

/// Name shown on card @p card of the ground folder.
[[nodiscard]] constexpr std::string_view editorGroundCardName(size_t card) {
  return card < EDITOR_TERRAIN_COUNT ? EDITOR_TERRAINS[card].name
                                     : EDITOR_ERASER_NAME;
}

/// The terrain numbered @p terrain, or nothing for bare ground and for a
/// number past the last.
[[nodiscard]] const EditorTerrain* editorTerrainAt(uint8_t terrain);

/// The word terrain number @p terrain is named by: `none` for bare ground.
/// Empty for a number past the last.
[[nodiscard]] std::string_view editorTerrainWord(uint8_t terrain);

/// The terrain number @p word names — `sand`, or `tile:sand` as a file
/// writes it — with `none` naming bare ground. Nothing for a word no
/// terrain has.
[[nodiscard]] std::optional<uint8_t> editorTerrainNamed(std::string_view word);

}  // namespace eng::editor
