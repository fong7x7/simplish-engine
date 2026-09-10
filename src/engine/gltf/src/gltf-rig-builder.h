#pragma once

/// @file gltf-rig-builder.h
/// @brief A glTF skin and the node hierarchy above it, as a skeleton and a
/// skin.
/// @par Threading
/// Pure function over a loaded document.

#include "gltf-document.h"

#include <cstdint>
#include <engine/animation/skeleton.h>
#include <engine/animation/skin.h>
#include <engine/core/expected-polyfill.h>
#include <engine/gltf/gltf-load-error.h>
#include <limits>
#include <vector>

namespace eng::gltf {

/// `GltfRig::joint_of_node` entry for a node outside the skeleton.
inline constexpr uint32_t GLTF_NOT_A_JOINT =
    std::numeric_limits<uint32_t>::max();

/// A skeleton and skin built from a glTF document, and the map from the
/// document's node numbers to the skeleton's joints that clips are built
/// through.
/// @thread_safety Immutable value type once built.
struct GltfRig {
  /// Every skin joint and every node above one, parents first.
  animation::Skeleton skeleton;
  /// The skin, its joints renumbered into the skeleton.
  animation::Skin skin;
  /// Skeleton joint for each document node, or `GLTF_NOT_A_JOINT`.
  std::vector<uint32_t> joint_of_node;
};

/// Build the rig for skin @p skin of @p document.
///
/// The skeleton holds the skin's joints and all their ancestors — glTF
/// computes a joint's transform from the scene root down, so a parent that
/// is not itself a joint, such as an exporter's armature node, still moves
/// it. glTF lists nodes in any order; the skeleton is reordered depth-first
/// so every parent precedes its children.
[[nodiscard]] std::expected<GltfRig, GltfLoadError>
buildGltfRig(const GltfDocument& document, size_t skin);

}  // namespace eng::gltf
