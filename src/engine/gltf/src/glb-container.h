#pragma once

/// @file glb-container.h
/// @brief Splitting a binary glTF container into its JSON and binary chunks.
/// @par Threading
/// Pure functions over caller-owned bytes.

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace eng::gltf {

/// The two chunks of a `.glb`, viewing the container's own bytes.
/// @thread_safety Immutable view; valid while the container's bytes are.
struct GlbChunks {
  /// The JSON chunk: the glTF document itself.
  std::string_view json;
  /// The binary chunk, which buffer 0 refers to when it has no URI. Empty
  /// when the container has none.
  std::span<const uint8_t> bin;
};

/// Whether @p bytes start with the GLB magic, "glTF".
[[nodiscard]] bool looksLikeGlb(std::span<const uint8_t> bytes);

/// The chunks of a GLB container: a 12-byte header (magic, version 2,
/// length), a JSON chunk, and optionally a binary one, each an 8-byte
/// length-and-type header before its data. Nullopt for anything else,
/// including a chunk that runs past the end of the bytes.
[[nodiscard]] std::optional<GlbChunks> splitGlb(std::span<const uint8_t> bytes);

}  // namespace eng::gltf
