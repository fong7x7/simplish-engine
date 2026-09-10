#pragma once

/// @file gltf-node-pose.h
/// @brief A glTF node's local transform, as a joint pose.
/// @par Threading
/// Pure functions.

#include "gltf-json.h"

#include <engine/animation/joint-pose.h>
#include <engine/math/mat4.h>

namespace eng::gltf {

/// The local pose node @p node declares: its `translation`, `rotation`, and
/// `scale`, each defaulting to glTF's identity, or its `matrix` taken apart
/// into those three when it gives one instead.
[[nodiscard]] animation::JointPose gltfNodePose(const Json& node);

/// A translate-rotate-scale matrix taken apart into its three parts.
///
/// The scale is each basis column's length and the rotation is what is left
/// once it is divided out. A matrix with shear has no exact answer; this
/// gives the nearest rotation, which is all glTF allows a joint's matrix to
/// hold anyway, since it must be decomposable to be animated.
[[nodiscard]] animation::JointPose decomposeJointMatrix(const Mat4& m);

}  // namespace eng::gltf
