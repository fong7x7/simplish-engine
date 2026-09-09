#pragma once

/// @file mtl-loader.h
/// @brief Wavefront MTL reader, for the one map a mesh is drawn with.
/// @par Threading Thread-safe (pure function over the input text).

#include <optional>
#include <string>
#include <string_view>

namespace eng {

/// The diffuse map a material library names, as written in the file.
///
/// `map_Kd` is the only statement read. The shader has one texture and no
/// material model to hang a specular or a normal map on, so reading them
/// would be collecting data nothing can draw. The path comes back exactly
/// as the file wrote it — relative, usually — because resolving it needs
/// the directory the file was read from, which a pure function does not
/// have.
///
/// @param text      The MTL file's contents.
/// @param material  Material to read, or empty for the first one that
///                  names a map. An OBJ that declared `usemtl` names one;
///                  an OBJ that did not still gets the library's only map,
///                  which is what a single-material export looks like.
/// @return The map's path, or nullopt when no such material names one.
[[nodiscard]] std::optional<std::string>
parseMtlDiffuseMap(std::string_view text, std::string_view material);

}  // namespace eng
