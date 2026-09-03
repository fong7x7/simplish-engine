#pragma once

/// @file editor-asset.h
/// @brief One importable asset the editor's asset panel lists.
/// @par Threading Main-thread-only.

#include <engine/math/vec3.h>
#include <engine/render-mesh/mesh-instance.h>
#include <filesystem>
#include <string>

namespace eng::editor {

/// A model on disk, and its GPU mesh once something has placed it.
///
/// The mesh is uploaded lazily: scanning a directory should cost a
/// `directory_iterator` pass, not a parse and a GPU allocation per file,
/// and most assets in a project are never placed in any one session.
/// @thread_safety Main-thread-only.
struct EditorAsset {
  /// Display name, taken from the file stem.
  std::string name;
  /// Absolute path to the source file.
  std::filesystem::path path;
  /// Uploaded mesh, or `MESH_GPU_INVALID` until first placed.
  MeshGpuId mesh = MESH_GPU_INVALID;
  /// Minimum bounds corner, in world orientation. Valid once uploaded.
  Vec3 min{};
  /// Maximum bounds corner, in world orientation. Valid once uploaded.
  Vec3 max{};
  /// Set when a load was attempted and failed, so it is not retried on
  /// every drop and the panel can show the asset as unusable.
  bool load_failed = false;
};

}  // namespace eng::editor
