#include "gltf-accessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>

using Catch::Approx;
using eng::gltf::GltfDocument;
using eng::gltf::Json;
using eng::gltf::readAccessorFloats;
using eng::gltf::readAccessorUints;

namespace {

/// A document over one 32-byte buffer holding @p bytes, with one view over
/// all of it and one accessor described by @p accessor.
GltfDocument docWith(std::vector<uint8_t> bytes, Json accessor) {
  GltfDocument doc;
  // Arrays built explicitly: nlohmann reads a braced single object as that
  // object, not as an array holding it.
  doc.root["bufferViews"] = Json::array();
  doc.root["bufferViews"].push_back(
      {{"buffer", 0}, {"byteLength", bytes.size()}});
  doc.root["accessors"] = Json::array();
  doc.root["accessors"].push_back(std::move(accessor));
  doc.buffers.push_back(std::move(bytes));
  return doc;
}

/// @p floats laid out as bytes.
std::vector<uint8_t> floatBytes(const std::vector<float>& floats) {
  std::vector<uint8_t> bytes(floats.size() * sizeof(float));
  std::memcpy(bytes.data(), floats.data(), bytes.size());
  return bytes;
}

}  // namespace

TEST_CASE("a float accessor reads its elements in order") {
  const auto doc =
      docWith(floatBytes({1, 2, 3, 4, 5, 6}), {{"bufferView", 0},
                                               {"componentType", 5126},
                                               {"count", 2},
                                               {"type", "VEC3"}});
  const auto values = readAccessorFloats(doc, 0, 3);
  REQUIRE(values == std::vector<float>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("a strided view skips the bytes between elements") {
  // Interleaved: each element is two floats, of which the accessor reads
  // the first, eight bytes apart.
  auto doc =
      docWith(floatBytes({1, 99, 2, 99, 3, 99}), {{"bufferView", 0},
                                                  {"componentType", 5126},
                                                  {"count", 3},
                                                  {"type", "SCALAR"}});
  doc.root["bufferViews"][0]["byteStride"] = 8;
  REQUIRE(readAccessorFloats(doc, 0, 1) == std::vector<float>{1, 2, 3});
}

TEST_CASE("normalised integers map onto the unit range") {
  const auto doc = docWith({0, 255, 51, 0}, {{"bufferView", 0},
                                             {"componentType", 5121},
                                             {"normalized", true},
                                             {"count", 1},
                                             {"type", "VEC4"}});
  const auto values = readAccessorFloats(doc, 0, 4);
  REQUIRE(values.has_value());
  REQUIRE((*values)[1] == 1.0f);
  REQUIRE((*values)[2] == Approx(0.2f));
}

TEST_CASE("normalised signed shorts clamp at minus one") {
  // -32768 has no positive twin, so it reads as -1 rather than below it.
  const auto doc = docWith({0x00, 0x80, 0xFF, 0x7F}, {{"bufferView", 0},
                                                      {"componentType", 5122},
                                                      {"normalized", true},
                                                      {"count", 1},
                                                      {"type", "VEC2"}});
  const auto values = readAccessorFloats(doc, 0, 2);
  REQUIRE(values == std::vector<float>{-1.0f, 1.0f});
}

TEST_CASE("unsigned indices read at every width") {
  const auto doc = docWith({1, 0, 2, 0, 3, 0}, {{"bufferView", 0},
                                                {"componentType", 5123},
                                                {"count", 3},
                                                {"type", "SCALAR"}});
  REQUIRE(readAccessorUints(doc, 0, 1) == std::vector<uint32_t>{1, 2, 3});
}

TEST_CASE("indices cannot be read from a float accessor") {
  const auto doc = docWith(floatBytes({1}), {{"bufferView", 0},
                                             {"componentType", 5126},
                                             {"count", 1},
                                             {"type", "SCALAR"}});
  REQUIRE_FALSE(readAccessorUints(doc, 0, 1).has_value());
}

TEST_CASE("an accessor of the wrong shape is refused") {
  const auto doc = docWith(floatBytes({1, 2, 3}), {{"bufferView", 0},
                                                   {"componentType", 5126},
                                                   {"count", 1},
                                                   {"type", "VEC3"}});
  REQUIRE_FALSE(readAccessorFloats(doc, 0, 4).has_value());
}

TEST_CASE("an accessor past the end of its view is refused") {
  const auto doc = docWith(floatBytes({1, 2}), {{"bufferView", 0},
                                                {"componentType", 5126},
                                                {"count", 3},
                                                {"type", "SCALAR"}});
  REQUIRE_FALSE(readAccessorFloats(doc, 0, 1).has_value());
}

TEST_CASE("a view past the end of its buffer is refused") {
  auto doc = docWith(floatBytes({1, 2}), {{"bufferView", 0},
                                          {"componentType", 5126},
                                          {"count", 1},
                                          {"type", "SCALAR"}});
  doc.root["bufferViews"][0]["byteOffset"] = 100;
  REQUIRE_FALSE(readAccessorFloats(doc, 0, 1).has_value());
}

TEST_CASE("a sparse accessor is refused") {
  const auto doc = docWith(floatBytes({1}), {{"bufferView", 0},
                                             {"componentType", 5126},
                                             {"count", 1},
                                             {"type", "SCALAR"},
                                             {"sparse", {{"count", 1}}}});
  REQUIRE_FALSE(readAccessorFloats(doc, 0, 1).has_value());
}
