#pragma once

/// @file sprite-quad.h
/// @brief The quad one frame of a sprite sheet is drawn on.
/// @par Threading Thread-safe (pure function over value types).

#include <engine/render-mesh/mesh-data.h>
#include <engine/render-sprite/sprite-uv-rect.h>

namespace eng {

/// A billboard's quad, showing the part of a sheet @p uv names.
///
/// One tile wide and one tile tall, standing in the local X–Z plane with
/// its base on the origin: the caller's model matrix is what turns it to
/// face the camera and sizes it, so this geometry does not depend on the
/// projection and can be uploaded once per frame of a sheet rather than
/// once per billboard.
///
/// The normal is local +Y, which is nothing the quad's own geometry says —
/// a flat quad's normal would be whichever way it happens to be wound.
/// Local +Y is the column of the model matrix that carries no vertex (every
/// vertex has y = 0), so a caller can aim the normal along the view ray
/// there and have the sprite shaded as a surface facing the camera, without
/// leaning the quad itself out of the upright.
///
/// Drawn through the mesh pipeline, whose fragment stage drops texels below
/// `MESH_ALPHA_CUTOFF` — the alpha-test cutout [ADR-003] calls for, which
/// is what gives a sprite hard edges that write depth correctly.
[[nodiscard]] MeshData makeSpriteQuadMesh(const SpriteUvRect& uv);

}  // namespace eng
