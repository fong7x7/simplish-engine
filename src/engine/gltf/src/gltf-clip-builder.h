#pragma once

/// @file gltf-clip-builder.h
/// @brief A glTF document's animations, as clips over a skeleton.
/// @par Threading
/// Pure function over a loaded document.

#include "gltf-document.h"

#include <cstdint>
#include <engine/animation/animation-clip.h>
#include <engine/core/expected-polyfill.h>
#include <engine/gltf/gltf-load-error.h>
#include <span>
#include <vector>

namespace eng::gltf {

/// Every animation in @p document, with its channels renumbered from nodes
/// to skeleton joints through @p joint_of_node.
///
/// A channel on a node outside the skeleton, or on morph weights, is
/// dropped: nothing here could play it. An animation left with no channels
/// is still a clip, which poses the rest pose, so a clip list lines up with
/// the file's animation list. An animation with no name is named for its
/// position, "animation 0" onwards.
[[nodiscard]] std::expected<std::vector<animation::AnimationClip>,
                            GltfLoadError>
buildGltfClips(const GltfDocument& document,
               std::span<const uint32_t> joint_of_node);

}  // namespace eng::gltf
