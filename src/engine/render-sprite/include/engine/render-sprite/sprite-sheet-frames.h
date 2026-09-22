#pragma once

/// @file sprite-sheet-frames.h
/// @brief Which frame of a sheet is showing, and where it sits in the image.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/render-sprite/sprite-sheet.h>
#include <engine/render-sprite/sprite-uv-rect.h>

namespace eng {

/// How many frames @p sheet actually plays: its own count, held to the
/// cells the grid has, and never fewer than one.
///
/// One rather than zero for an empty sheet, because every caller past this
/// point divides by it or indexes with it, and a sheet nobody has filled in
/// yet should show its first cell rather than nothing at all.
[[nodiscard]] uint16_t spriteSheetFrameCount(const SpriteSheet& sheet);

/// The frame @p sheet shows @p seconds into its loop.
///
/// Loops for ever: a billboard is scenery, and a sheet that stopped on its
/// last frame would need a rule for what to do then that nothing in a level
/// is able to give it. A sheet with no speed holds frame zero.
///
/// @p seconds is presentation time — the render frame's clock, never the
/// tick's
/// ([ADR-002](../../../../../docs/decisions/ADR-002-fixed-timestep-determinism.md)).
[[nodiscard]] uint16_t spriteFrameAt(const SpriteSheet& sheet, double seconds);

/// Where frame @p frame sits in the sheet's image, counted left to right
/// and then down. A frame past the last one wraps, so no caller can ask for
/// coordinates outside the image.
[[nodiscard]] SpriteUvRect spriteFrameUv(const SpriteSheet& sheet,
                                         uint16_t frame);

/// The pixel size of one frame of @p sheet, cut from an image @p pixels
/// across and down.
///
/// What a billboard's proportions come from: a frame is whatever shape the
/// artist drew it, and the world size it is given has to keep that shape.
[[nodiscard]] Vec2 spriteFramePixels(const SpriteSheet& sheet, Vec2 pixels);

/// @p uv pulled in by half a texel on every side, given a texel @p u_texel
/// wide and @p v_texel tall in texture coordinates.
///
/// Sampling is bilinear, so a coordinate exactly on the seam between two
/// cells mixes both of them and the frame wears a sliver of its neighbour
/// down one edge. Half a texel is the smallest inset that cannot: it puts
/// every sampled point at or inside the first texel's centre.
[[nodiscard]] SpriteUvRect insetSpriteUv(const SpriteUvRect& uv, float u_texel,
                                         float v_texel);

}  // namespace eng
