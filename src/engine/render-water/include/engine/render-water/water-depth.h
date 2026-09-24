#pragma once

/// @file water-depth.h
/// @brief How deep painted water is: the unit a level stores it in, and how
/// it shallows towards a bank.
/// @par Threading Thread-safe (immutable data and pure functions).

#include <algorithm>
#include <cstdint>

namespace eng {

/// Tiles of depth one stored unit is worth. A cell's depth is a byte of
/// these, so water runs from a sixteenth of a tile to almost sixteen, and
/// a zero is no water at all.
inline constexpr float WATER_DEPTH_STEP = 1.0f / 16.0f;

/// The depth water is painted at when nobody has chosen one, in tiles: a
/// pond. Also what water from a level saved before depth existed reads as.
inline constexpr float WATER_DEFAULT_DEPTH = 1.0f;

/// The deepest a cell can be, in tiles: the largest byte's worth.
inline constexpr float WATER_MAX_DEPTH = 255.0f * WATER_DEPTH_STEP;

/// The shortest run from a bank to a cell's full depth, in tiles: however
/// shallow the water, its very edge is shallower.
inline constexpr float WATER_BANK_MIN_TILES = 0.3f;

/// How much further the run to full depth reaches per tile of depth: a
/// deep lake shelves away from its beach where a puddle is full at once.
inline constexpr float WATER_BANK_TILES_PER_DEPTH = 0.5f;

/// The longest run from a bank to full depth, in tiles; the shore distance
/// every sample carries stops counting there (`WATER_SHORE_TILES`).
inline constexpr float WATER_BANK_MAX_TILES = 2.0f;

/// The depth, in tiles, a stored byte @p units means: 0 is dry.
[[nodiscard]] constexpr float waterDepthTiles(uint8_t units) {
  return static_cast<float>(units) * WATER_DEPTH_STEP;
}

/// The byte @p tiles of depth is stored as: the nearest step, from one step
/// to the deepest.
[[nodiscard]] constexpr uint8_t waterDepthUnits(float tiles) {
  const float steps = std::clamp(tiles / WATER_DEPTH_STEP, 1.0f, 255.0f);
  return static_cast<uint8_t>(steps + 0.5f);
}

/// How much of water @p depth tiles deep is there @p shore tiles from the
/// nearest bank: none at the bank, all of it a run out that grows with the
/// depth, eased at both ends. Every water shader restates this.
[[nodiscard]] constexpr float waterBankShelf(float depth, float shore) {
  const float run =
      std::clamp(WATER_BANK_MIN_TILES + WATER_BANK_TILES_PER_DEPTH * depth,
                 WATER_BANK_MIN_TILES, WATER_BANK_MAX_TILES);
  const float t = std::clamp(shore / run, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace eng
