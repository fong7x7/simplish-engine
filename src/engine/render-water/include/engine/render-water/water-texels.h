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

/// Write @p field into @p texels, one RGBA8 texel a sample in the field's
/// own order — row by row from the south-west — resized to fit.
///
/// Every channel is unorm about a middle of 127.5, so still water is grey:
///
/// | Channel | Holds |
/// |---|---|
/// | R, G | The slope along x and y, ÷ `WATER_SLOPE_RANGE`, from central
/// differences | | B | The level, ÷ `WATER_LEVEL_RANGE` | | A | Shore distance
/// ÷ `WATER_SHORE_TILES`, from 0 on land to 1 in the deep |
///
/// The slope is worked out here rather than in the shader so a texel is
/// enough for a fragment's normal: one filtered read, not five.
void writeWaterTexels(const WaterField& field, std::vector<uint8_t>& texels);

}  // namespace eng
