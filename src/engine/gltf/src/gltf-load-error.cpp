#include <engine/gltf/gltf-load-error.h>

namespace eng::gltf {

std::string_view gltfLoadErrorMessage(GltfLoadError error) {
  switch (error) {
    case GltfLoadError::UNREADABLE:
      return "the file, or a buffer beside it, could not be read";
    case GltfLoadError::MALFORMED:
      return "not a glTF document";
    case GltfLoadError::UNSUPPORTED_VERSION:
      return "not glTF 2.0";
    case GltfLoadError::UNSUPPORTED_EXTENSION:
      return "requires a glTF extension the loader does not read "
             "(export without compression)";
    case GltfLoadError::MISSING_BUFFER:
      return "a buffer is missing or shorter than declared";
    case GltfLoadError::BAD_ACCESSOR:
      return "an accessor is sparse, the wrong shape, or out of bounds";
    case GltfLoadError::NO_SKINNED_MESH:
      return "no skinned mesh (export static models as OBJ)";
    case GltfLoadError::TOO_MANY_JOINTS:
      return "the skin has more than 80 joints";
    case GltfLoadError::BAD_SKIN:
      return "the skin or node hierarchy is inconsistent";
    case GltfLoadError::NO_TRIANGLES:
      return "the skinned mesh has no triangles";
  }
  return "unknown glTF error";
}

}  // namespace eng::gltf
