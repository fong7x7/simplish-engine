#pragma once

/// @file obj-loader.h
/// @brief Wavefront OBJ reader for static meshes.
/// @par Threading Main thread only (`loadObjMesh` reads from disk).

#include <engine/render-mesh/mesh-data.h>
#include <filesystem>
#include <optional>
#include <string_view>

namespace eng {

/// Parse OBJ text into a triangle mesh.
///
/// Handles `v`, `vn`, and `f` with any of the `a`, `a/b`, `a//c`, `a/b/c`
/// corner forms, positive or negative indices, and faces with more than
/// three corners (triangulated as a fan). Texture coordinates, materials,
/// groups, and smoothing are skipped: nothing downstream reads them yet.
///
/// Faces without normals get a computed flat normal, which costs vertex
/// sharing for those faces but renders correctly rather than black.
///
/// Returns nullopt when the text yields no triangles.
/// @thread_safety Thread-safe (pure function over the input text).
[[nodiscard]] std::optional<MeshData> parseObjMesh(std::string_view text);

/// Read and parse an OBJ file. Returns nullopt when it cannot be read or
/// contains no triangles.
[[nodiscard]] std::optional<MeshData>
loadObjMesh(const std::filesystem::path& path);

}  // namespace eng
