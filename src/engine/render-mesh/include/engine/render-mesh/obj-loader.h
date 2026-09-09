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
/// Handles `v`, `vt`, `vn`, and `f` with any of the `a`, `a/b`, `a//c`,
/// `a/b/c` corner forms, positive or negative indices, and faces with more
/// than three corners (triangulated as a fan). `mtllib` and the first
/// `usemtl` are recorded; groups and smoothing are skipped, since nothing
/// downstream reads them yet.
///
/// The material is recorded, not resolved: turning it into a texture means
/// reading a second file from the directory the first came from, which
/// only `loadObjMesh` below can do.
///
/// Faces without normals get a computed flat normal, which costs vertex
/// sharing for those faces but renders correctly rather than black.
///
/// Returns nullopt when the text yields no triangles.
/// @thread_safety Thread-safe (pure function over the input text).
[[nodiscard]] std::optional<MeshData> parseObjMesh(std::string_view text);

/// Read and parse an OBJ file, resolving its diffuse map if it names one.
///
/// The material library is read from beside the OBJ and its `map_Kd`
/// resolved against the same directory; `MeshData::texture_path` is left
/// empty when any step of that finds nothing, including when the image the
/// material names is not actually on disk.
///
/// Returns nullopt when the OBJ cannot be read or contains no triangles.
[[nodiscard]] std::optional<MeshData>
loadObjMesh(const std::filesystem::path& path);

}  // namespace eng
