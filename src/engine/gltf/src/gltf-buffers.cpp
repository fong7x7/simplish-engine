#include "gltf-buffers.h"

#include "gltf-uri.h"

#include <fstream>
#include <iterator>

namespace eng::gltf {

namespace {

  /// The bytes buffer @p entry names, or nothing when they cannot be found.
  std::optional<std::vector<uint8_t>>
  bufferBytes(const Json& entry, std::span<const uint8_t> glb_bin,
              const std::filesystem::path& base_dir) {
    const Json* uri = jsonMember(entry, "uri");
    if (uri == nullptr) {
      return std::vector<uint8_t>(glb_bin.begin(), glb_bin.end());
    }
    if (!uri->is_string()) {
      return std::nullopt;
    }
    const auto& text = uri->get_ref<const std::string&>();
    if (isDataUri(text)) {
      return decodeDataUri(text);
    }
    return readFileBytes(base_dir / uriToPath(text));
  }

}  // namespace

std::optional<std::vector<uint8_t>>
readFileBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                              std::istreambuf_iterator<char>());
}

std::optional<GltfLoadError> loadGltfBuffers(GltfDocument& document,
                                             std::span<const uint8_t> glb_bin) {
  const size_t count = jsonArraySize(document.root, "buffers");
  document.buffers.clear();
  for (size_t i = 0; i < count; ++i) {
    const Json& entry = *jsonElement(document.root, "buffers", i);
    auto bytes = bufferBytes(entry, glb_bin, document.base_dir);
    const size_t declared = jsonIndex(entry, "byteLength").value_or(0);
    if (!bytes || bytes->size() < declared) {
      return GltfLoadError::MISSING_BUFFER;
    }
    document.buffers.push_back(std::move(*bytes));
  }
  return std::nullopt;
}

}  // namespace eng::gltf
