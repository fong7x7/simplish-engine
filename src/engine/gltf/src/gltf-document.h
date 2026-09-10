#pragma once

/// @file gltf-document.h
/// @brief A parsed glTF document with its buffers resolved: everything the
/// builders read from.
/// @par Threading
/// Immutable once built; read from any thread.

#include "gltf-json.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace eng::gltf {

/// The JSON, every buffer's bytes, and where relative URIs resolve.
/// @thread_safety Immutable once built.
struct GltfDocument {
  /// The document root.
  Json root;
  /// Each of `root["buffers"]`, loaded, in order.
  std::vector<std::vector<uint8_t>> buffers;
  /// Directory the document was read from, for image URIs.
  std::filesystem::path base_dir;
};

}  // namespace eng::gltf
