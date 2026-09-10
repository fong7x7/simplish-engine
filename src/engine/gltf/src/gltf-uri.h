#pragma once

/// @file gltf-uri.h
/// @brief The two kinds of URI a glTF file uses: inline `data:` URIs, and
/// relative paths to files beside it.
/// @par Threading
/// Pure functions.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::gltf {

/// Whether @p uri carries its bytes inline rather than naming a file.
[[nodiscard]] bool isDataUri(std::string_view uri);

/// The bytes of a base64 `data:` URI — `data:<type>;base64,<payload>`, the
/// only form glTF writes. Nullopt for any other form, or a payload with a
/// character outside the base64 alphabet.
[[nodiscard]] std::optional<std::vector<uint8_t>>
decodeDataUri(std::string_view uri);

/// A relative URI as a path: percent escapes such as `%20` turned back into
/// the bytes they stand for, since exporters escape the spaces in a file's
/// name and the file on disk has none of the escapes.
[[nodiscard]] std::string uriToPath(std::string_view uri);

}  // namespace eng::gltf
