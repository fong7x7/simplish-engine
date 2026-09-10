#pragma once

/// @file gltf-load-error.h
/// @brief Why a glTF file did not become a skinned model.
/// @par Threading
/// Thread-safe (enum and a pure function).

#include <cstdint>
#include <string_view>

namespace eng::gltf {

/// Why loading a glTF model failed. Specific enough that an editor can say
/// what to fix in the exporter rather than only that something is wrong.
enum class GltfLoadError : uint8_t {
  /// The file, or a buffer it names, could not be read from disk.
  UNREADABLE,
  /// Not JSON, not a GLB container, or JSON that is not a glTF document.
  MALFORMED,
  /// A glTF version other than 2.
  UNSUPPORTED_VERSION,
  /// The file requires an extension this loader does not implement, such
  /// as Draco compression.
  UNSUPPORTED_EXTENSION,
  /// A buffer is missing, or shorter than the file says it is.
  MISSING_BUFFER,
  /// An accessor points outside its buffer, has the wrong shape for what
  /// reads it, or is sparse.
  BAD_ACCESSOR,
  /// No node has both a mesh and a skin — a static model, which is an OBJ's
  /// job today.
  NO_SKINNED_MESH,
  /// The skin names more joints than one draw can carry
  /// (`MESH_MAX_SKIN_JOINTS`).
  TOO_MANY_JOINTS,
  /// The skin or node hierarchy is inconsistent: a vertex names a joint the
  /// skin does not have, a node has two parents, or the hierarchy loops.
  BAD_SKIN,
  /// The skinned mesh has no triangles.
  NO_TRIANGLES,
};

/// A sentence saying what went wrong, for a status line or a log.
[[nodiscard]] std::string_view gltfLoadErrorMessage(GltfLoadError error);

}  // namespace eng::gltf
