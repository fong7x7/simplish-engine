#pragma once

/// @file test-gltf.h
/// @brief Building glTF documents in memory for the loader's tests.
/// @par Threading
/// Single-threaded test helper.

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace eng::gltf::test {

/// A glTF document under construction: typed arrays appended to one buffer,
/// each getting its own buffer view and accessor, and the JSON around them
/// written by the test directly.
class TestGltf {
public:
  TestGltf();

  /// Append @p values as a float accessor of glTF type @p type ("VEC3" and
  /// so on). Returns the accessor's index.
  size_t addFloats(const std::vector<float>& values, const char* type);

  /// Append @p values as an unsigned-byte accessor.
  size_t addBytes(const std::vector<uint8_t>& values, const char* type);

  /// Append @p values as an unsigned-short accessor.
  size_t addShorts(const std::vector<uint16_t>& values, const char* type);

  /// The document, for the test to add nodes, meshes, and the rest to.
  nlohmann::json& doc() { return doc_; }

  /// The buffer every accessor reads, as a `.bin` file beside the document
  /// would hold it.
  [[nodiscard]] const std::vector<uint8_t>& bin() const { return bin_; }

  /// The document as `.gltf` text, its buffer inline as a base64 data URI.
  [[nodiscard]] std::string gltfText() const;

  /// The document as a `.glb` container, its buffer as the binary chunk.
  [[nodiscard]] std::vector<uint8_t> glbBytes() const;

private:
  /// Append raw bytes to the buffer as a new buffer view; returns its index.
  size_t addView(const void* data, size_t bytes);

  /// Add an accessor of @p count elements of @p type over view @p view,
  /// leaving its component type for the caller; returns its index.
  size_t addAccessor(size_t view, size_t count, const char* type);

  /// The document so far, without its buffer.
  nlohmann::json doc_;
  /// Every array appended, back to back, each started on a 4-byte boundary.
  std::vector<uint8_t> bin_;
};

/// A two-joint arm, Y-up, as a document a loader should accept.
///
/// Nodes are listed out of order — elbow, shoulder, armature, mesh — so the
/// skeleton has to be reordered. The armature is a plain node above the
/// shoulder, not a joint. The shoulder is at the origin and the elbow one
/// unit up +Y from it; a triangle's base hangs from the shoulder and its tip
/// from the elbow; and a clip named "swing" turns the shoulder a quarter
/// turn about Z over one second.
[[nodiscard]] TestGltf armDocument();

}  // namespace eng::gltf::test
