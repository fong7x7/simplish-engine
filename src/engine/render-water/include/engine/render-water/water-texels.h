#pragma once

/// @file water-texels.h
/// @brief A water field packed into the texture the water shader reads.
/// @par Threading Thread-safe (pure function over value types).

#include <cstdint>
#include <engine/render-water/water-field.h>
#include <vector>

namespace eng {

/// The steepest slope a texel records, either way. Steeper is clamped: a
/// ripple that steep is already white with foam.
inline constexpr float WATER_SLOPE_RANGE = 1.0f;

/// The highest crest and deepest trough a texel records, in tiles.
inline constexpr float WATER_LEVEL_RANGE = 0.1f;

/// Bytes per texel: `RGB_A8_UNORM`.
inline constexpr uint32_t WATER_TEXEL_BYTES = 4;

/// Write @p field's motion into @p texels, one RGBA8 texel a sample in the
/// field's own order — row by row from the south-west — resized to fit.
/// Rewritten every frame. The channels:
///
/// - R, G: the slope along x and y ÷ `WATER_SLOPE_RANGE`, from central
///   differences, about a middle of 127.5.
/// - B: the level ÷ `WATER_LEVEL_RANGE`, about the same middle.
/// - A: the foam on it, from 0 for none to 1.
///
/// The slope is worked out here rather than in the shader so a texel is
/// enough for a fragment's normal: one filtered read, not five.
void writeWaterTexels(const WaterField& field, std::vector<uint8_t>& texels);

/// Write what does not move about @p field into @p texels, in the same
/// order: rewritten only when the field is shaped anew. The channels:
///
/// - R: about a middle of 127.5 — above it, a wet sample's shore distance
///   ÷ `WATER_SHORE_TILES`, up to 1 in the deep; below it, a dry one's
///   distance to the water ÷ `WATER_WET_TILES`, down to 0 on dry land.
///   Each is nothing where the other is not, so one channel holds both.
/// - G, B: the way the water flows, along x and y ÷ `WATER_MAX_FLOW_SPEED`,
///   about the same middle.
/// - A: how thick the water is, from 0 for water to 1 for the thickest.
void writeWaterStillTexels(const WaterField& field,
                           std::vector<uint8_t>& texels);

}  // namespace eng
