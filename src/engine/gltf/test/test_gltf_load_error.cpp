#include <catch2/catch_test_macros.hpp>
#include <engine/gltf/gltf-load-error.h>
#include <set>
#include <string>

using eng::gltf::GltfLoadError;
using eng::gltf::gltfLoadErrorMessage;

TEST_CASE("every load error has its own message") {
  const GltfLoadError all[] = {GltfLoadError::UNREADABLE,
                               GltfLoadError::MALFORMED,
                               GltfLoadError::UNSUPPORTED_VERSION,
                               GltfLoadError::UNSUPPORTED_EXTENSION,
                               GltfLoadError::MISSING_BUFFER,
                               GltfLoadError::BAD_ACCESSOR,
                               GltfLoadError::NO_SKINNED_MESH,
                               GltfLoadError::TOO_MANY_JOINTS,
                               GltfLoadError::BAD_SKIN,
                               GltfLoadError::NO_TRIANGLES};
  std::set<std::string> seen;
  for (const GltfLoadError error : all) {
    const auto message = gltfLoadErrorMessage(error);
    REQUIRE_FALSE(message.empty());
    seen.emplace(message);
  }
  REQUIRE(seen.size() == std::size(all));
}
