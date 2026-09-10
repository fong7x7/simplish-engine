#pragma once

/// @file gltf-accessor.h
/// @brief Reading a glTF accessor's elements out of its buffer.
/// @par Threading
/// Pure functions over a loaded document.
///
/// An accessor is a typed, strided window into a buffer: element `i`,
/// component `c` sits at the view's offset plus the accessor's, plus `i`
/// times the stride, plus `c` times the component's size. These read that
/// window into flat arrays, `components` values per element, after checking
/// every byte of it lies inside the buffer the document loaded.

#include "gltf-document.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace eng::gltf {

/// Accessor @p accessor's elements as floats, @p components per element.
///
/// Integer components are converted: a `normalized` accessor maps its
/// range onto [0, 1] (or [-1, 1] for signed types) as glTF defines, and a
/// plain one is taken at its value. Nullopt when the accessor is missing,
/// sparse, has a different number of components, or runs past its buffer.
[[nodiscard]] std::optional<std::vector<float>>
readAccessorFloats(const GltfDocument& document, size_t accessor,
                   size_t components);

/// Accessor @p accessor's elements as unsigned integers, @p components per
/// element — for indices and joint numbers, which glTF stores as unsigned
/// bytes, shorts, or ints. Nullopt for a float accessor, or for any of the
/// failures `readAccessorFloats` refuses.
[[nodiscard]] std::optional<std::vector<uint32_t>>
readAccessorUints(const GltfDocument& document, size_t accessor,
                  size_t components);

/// The `count` accessor @p accessor declares, or zero when it has none.
[[nodiscard]] size_t accessorCount(const GltfDocument& document,
                                   size_t accessor);

}  // namespace eng::gltf
