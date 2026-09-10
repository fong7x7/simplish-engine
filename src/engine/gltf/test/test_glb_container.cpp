#include "glb-container.h"
#include "support/test-gltf.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using eng::gltf::looksLikeGlb;
using eng::gltf::splitGlb;
using eng::gltf::test::armDocument;

TEST_CASE("a GLB container splits into its JSON and binary chunks") {
  const auto g = armDocument();
  const auto glb = g.glbBytes();
  REQUIRE(looksLikeGlb(glb));
  const auto chunks = splitGlb(glb);
  REQUIRE(chunks.has_value());
  REQUIRE(chunks->json.starts_with('{'));
  REQUIRE(chunks->bin.size() >= g.bin().size());
}

TEST_CASE("JSON text is not a GLB container") {
  const std::string text = armDocument().gltfText();
  const std::vector<uint8_t> bytes(text.begin(), text.end());
  REQUIRE_FALSE(looksLikeGlb(bytes));
  REQUIRE_FALSE(splitGlb(bytes).has_value());
}

TEST_CASE("a container of another version is refused") {
  auto glb = armDocument().glbBytes();
  glb[4] = 1;
  REQUIRE_FALSE(splitGlb(glb).has_value());
}

TEST_CASE("a chunk that runs past the end is refused") {
  auto glb = armDocument().glbBytes();
  glb.resize(40);
  REQUIRE_FALSE(splitGlb(glb).has_value());
}

TEST_CASE("a container with no binary chunk has an empty one") {
  auto glb = armDocument().glbBytes();
  uint32_t json_length = 0;
  std::memcpy(&json_length, glb.data() + 12, sizeof(json_length));
  glb.resize(12 + 8 + json_length);
  const auto chunks = splitGlb(glb);
  REQUIRE(chunks.has_value());
  REQUIRE(chunks->bin.empty());
}
