#pragma once

/// @file editor-sprite-sheet-texture.h
/// @brief One sprite sheet image, as the editor holds it on the GPU.
/// @par Threading Main-thread-only.

#include <engine/math/vec2.h>
#include <engine/render/rhi-core-types.h>

namespace eng::editor {

/// A sheet image uploaded once and shown by every billboard that names it.
///
/// Kept by path rather than by billboard: two billboards on the same sheet
/// are the ordinary case — a level's crowd of the same plant — and a
/// texture per billboard would upload the same image as many times as it
/// was placed.
///
/// A sheet that could not be read is remembered as an entry with no
/// texture, so a missing or broken file is read once rather than on every
/// frame it is drawn in.
/// @thread_safety Main-thread-only.
struct EditorSpriteSheetTexture {
  /// The uploaded image, or `RHI_TEXTURE_INVALID` when it would not load.
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
  /// Its size in texels, which is what a frame's pixel size and the
  /// quads' half-texel inset are measured from.
  Vec2 pixels{};
};

}  // namespace eng::editor
