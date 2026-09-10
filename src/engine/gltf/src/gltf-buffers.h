#pragma once

/// @file gltf-buffers.h
/// @brief Loading every buffer a glTF document names.
/// @par Threading
/// Main thread only (reads from disk).

#include "gltf-document.h"

#include <cstdint>
#include <engine/gltf/gltf-load-error.h>
#include <optional>
#include <span>

namespace eng::gltf {

/// Fill @p document's buffers from its `buffers` array: a `data:` URI is
/// decoded, a relative URI is read from beside the document, and a buffer
/// with no URI is the GLB binary chunk @p glb_bin.
///
/// Each must hold at least the `byteLength` the document declares, since
/// every accessor bounds check is made against that. Returns the first
/// failure, or nothing when every buffer loaded.
[[nodiscard]] std::optional<GltfLoadError>
loadGltfBuffers(GltfDocument& document, std::span<const uint8_t> glb_bin);

/// Every byte of the file at @p path, or nothing when it cannot be read.
[[nodiscard]] std::optional<std::vector<uint8_t>>
readFileBytes(const std::filesystem::path& path);

}  // namespace eng::gltf
