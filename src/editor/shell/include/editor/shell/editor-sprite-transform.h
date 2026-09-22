#pragma once

/// @file editor-sprite-transform.h
/// @brief Sizing a sprite billboard and turning it to face the camera.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-sprite.h>
#include <editor/shell/iso-axes.h>
#include <engine/math/mat4.h>
#include <engine/math/vec2.h>
#include <engine/math/vec3.h>

namespace eng::editor {

/// How wide @p sprite stands, in tiles, showing a frame @p frame_pixels
/// across and down under @p axes.
///
/// Derived rather than authored, so one frame is never stretched: the
/// height is what a designer sets, and the width is whatever puts the
/// frame's own proportions on the screen. That takes the projection,
/// because the two screen axes are not at the same scale — a world unit
/// covers `isoAcrossPixels` pixels sideways and `IsoAxes::z_up` pixels
/// upward, and the ratio between them is the correction the 2026-09-09
/// amendment to ADR-003 left for the sprite pipeline to carry.
///
/// A sprite one tile tall is therefore drawn exactly as tall as a one-tile
/// cube beside it, and a square frame draws square.
[[nodiscard]] float editorSpriteWidth(const IsoAxes& axes,
                                      const EditorSprite& sprite,
                                      Vec2 frame_pixels);

/// The model matrix that puts @p sprite's quad where it stands, @p width
/// tiles across, facing the camera under @p axes.
///
/// The quad `makeSpriteQuadMesh` hands over is a unit square in its own
/// X–Z plane with its base on the origin, so this is the whole of what
/// makes it a billboard: its local X goes along `isoScreenRight`, its local
/// Z straight up the world, and its local +Y — the axis no vertex sits on,
/// which carries only the normal — along `isoProjectionRay`, so the quad is
/// shaded as a surface facing the camera without being leaned out of the
/// upright.
///
/// Upright rather than square-on to the view ray because the projection
/// never rotates
/// ([ADR-003](../../../../../docs/decisions/ADR-003-hybrid-iso-render-model.md)):
/// two fixed yaws are the only two facings a billboard ever needs, and an
/// upright quad's base stays exactly where the sprite stands, which is
/// where its depth is measured.
[[nodiscard]] Mat4 makeSpriteTransform(const IsoAxes& axes,
                                       const EditorSprite& sprite, float width);

}  // namespace eng::editor
