#include "glb-container.h"

#include <cstring>

namespace eng::gltf {

namespace {

  /// "glTF", read as a little-endian word.
  constexpr uint32_t GLB_MAGIC = 0x46546C67;
  /// The only container version glTF 2.0 defines.
  constexpr uint32_t GLB_VERSION = 2;
  /// "JSON", read as a little-endian word.
  constexpr uint32_t CHUNK_JSON = 0x4E4F534A;
  /// "BIN\0", read as a little-endian word.
  constexpr uint32_t CHUNK_BIN = 0x004E4942;
  /// Bytes before the first chunk.
  constexpr size_t HEADER_BYTES = 12;
  /// Bytes of a chunk's own length-and-type header.
  constexpr size_t CHUNK_HEADER_BYTES = 8;

  /// The little-endian word at @p offset. The caller has checked the four
  /// bytes are there.
  uint32_t wordAt(std::span<const uint8_t> bytes, size_t offset) {
    uint32_t word = 0;
    std::memcpy(&word, bytes.data() + offset, sizeof(word));
    return word;
  }

  /// The chunk starting at @p offset, if it has type @p type and fits.
  std::optional<std::span<const uint8_t>>
  chunkAt(std::span<const uint8_t> bytes, size_t offset, uint32_t type) {
    if (offset + CHUNK_HEADER_BYTES > bytes.size() ||
        wordAt(bytes, offset + 4) != type) {
      return std::nullopt;
    }
    const size_t length = wordAt(bytes, offset);
    if (length > bytes.size() - offset - CHUNK_HEADER_BYTES) {
      return std::nullopt;
    }
    return bytes.subspan(offset + CHUNK_HEADER_BYTES, length);
  }

}  // namespace

bool looksLikeGlb(std::span<const uint8_t> bytes) {
  return bytes.size() >= 4 && wordAt(bytes, 0) == GLB_MAGIC;
}

std::optional<GlbChunks> splitGlb(std::span<const uint8_t> bytes) {
  if (bytes.size() < HEADER_BYTES || !looksLikeGlb(bytes) ||
      wordAt(bytes, 4) != GLB_VERSION) {
    return std::nullopt;
  }
  const auto json = chunkAt(bytes, HEADER_BYTES, CHUNK_JSON);
  if (!json) {
    return std::nullopt;
  }
  GlbChunks chunks;
  chunks.json = {reinterpret_cast<const char*>(json->data()), json->size()};
  // Chunks are padded to four bytes, so the next starts right after.
  const size_t next = HEADER_BYTES + CHUNK_HEADER_BYTES + json->size();
  chunks.bin = chunkAt(bytes, next, CHUNK_BIN).value_or(chunks.bin);
  return chunks;
}

}  // namespace eng::gltf
