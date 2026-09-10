#pragma once

/// @file skinned-model-orientation.h
/// @brief Turning a Y-up skinned model into the engine's Z-up world.
/// @par Threading
/// Main thread only (mutates the model in place).

#include <engine/gltf/skinned-model.h>

namespace eng::gltf {

/// Rotate a Y-up skinned model into the engine's Z-up world, in place —
/// `orientYUpToZUp` for a model with bones.
///
/// glTF is Y-up by definition. Turning only the vertices would not be
/// enough: every clip still moves the joints in the old space, and the
/// first frame would swing the mesh back onto its side. So the turn `C`
/// goes three places at once — into the bind-pose vertices, above the
/// skeleton as the skin's root, and under each inverse bind matrix as
/// `C⁻¹` — which makes each skin matrix `C · M · C⁻¹` for the matrix `M` it
/// was. A vertex already turned by `C` then comes out exactly where the
/// unturned model would have put it, turned by `C`. Clips are untouched.
///
/// Maps model (x, y, z) to world (x, -z, y), as `orientYUpToZUp` does, and
/// updates the bounds to match.
void orientSkinnedYUpToZUp(SkinnedModel& model);

}  // namespace eng::gltf
