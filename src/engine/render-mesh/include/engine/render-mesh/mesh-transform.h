#pragma once

/// @file mesh-transform.h
/// @brief In-place fixes applied to a mesh after loading.

#include <engine/render-mesh/mesh-data.h>

namespace eng {

/// Rotate a Y-up mesh into the engine's Z-up world, in place.
///
/// Blender, Maya, and most OBJ exporters write Y-up by default while the
/// world here is Z-up, so a model would otherwise lie on its side. Baking
/// the rotation into the vertices rather than the instance transform keeps
/// normals correct without a normal matrix: what is left is a translate and
/// a uniform scale, neither of which changes a normal's direction.
///
/// Maps model (x, y, z) to world (x, -z, y), and updates the bounds to
/// match.
void orientYUpToZUp(MeshData& mesh);

}  // namespace eng
