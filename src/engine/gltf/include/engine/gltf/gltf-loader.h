#pragma once

/// @file gltf-loader.h
/// @brief Reading a rigged, animated model from glTF 2.0 — `.gltf` with its
/// buffers beside it or inline, or a self-contained `.glb`.
/// @par Threading
/// Main thread only (reads from disk).
///
/// What is read: the first node carrying both a mesh and a skin; every
/// triangle primitive of that mesh, merged into one mesh drawn with the
/// first primitive's base colour map; the skin's joints and every node
/// above them; and every animation's translation, rotation, and scale
/// channels on those nodes.
///
/// What is not: morph targets, cameras, lights, a second skinned mesh, and
/// images embedded in a buffer rather than beside the file. Channels that
/// animate nodes outside the skeleton are dropped. Required extensions
/// other than `KHR_mesh_quantization` are refused rather than half-read.
///
/// The model comes back in glTF's own space, Y-up. `orientSkinnedYUpToZUp`
/// turns it into the engine's.

#include <cstdint>
#include <engine/core/expected-polyfill.h>
#include <engine/gltf/gltf-load-error.h>
#include <engine/gltf/skinned-model.h>
#include <filesystem>
#include <span>
#include <string_view>

namespace eng::gltf {

/// A glTF document's JSON, parsed into a model.
///
/// @param json      The document text.
/// @param glb_bin   The GLB container's binary chunk, which a buffer with
///                  no URI refers to. Empty for a plain `.gltf`.
/// @param base_dir  Directory relative buffer and image URIs resolve
///                  against. Buffers given as `data:` URIs need none.
[[nodiscard]] std::expected<SkinnedModel, GltfLoadError>
parseGltfModel(std::string_view json, std::span<const uint8_t> glb_bin,
               const std::filesystem::path& base_dir);

/// A `.glb` container's bytes, parsed into a model.
[[nodiscard]] std::expected<SkinnedModel, GltfLoadError>
parseGlbModel(std::span<const uint8_t> glb,
              const std::filesystem::path& base_dir);

/// Read a `.gltf` or `.glb` file — told apart by its first four bytes, not
/// its name — and parse it, resolving buffers and images beside it.
[[nodiscard]] std::expected<SkinnedModel, GltfLoadError>
loadGltfModel(const std::filesystem::path& path);

}  // namespace eng::gltf
