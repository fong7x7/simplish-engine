#pragma once

/// @file gui-vertex-layout.h
/// @brief Vertex attribute layout constants for GUI rendering.
/// @par Threading
/// Constants only.

#include <cstddef>
#include <cstdint>

namespace eng::gui {

/// Number of vertex attributes in the GUI vertex format.
inline constexpr uint32_t GUI_VERTEX_ATTRIBUTE_COUNT = 9;
/// Byte stride between consecutive GUI vertices.
inline constexpr uint32_t GUI_VERTEX_STRIDE = 72;
/// Byte offset of `pos` (float2), location 0.
inline constexpr uint32_t GUI_ATTR_POS_OFFSET = 0;
/// Byte offset of `uv` (float2), location 1.
inline constexpr uint32_t GUI_ATTR_UV_OFFSET = 8;
/// Byte offset of `color` (uint), location 2.
inline constexpr uint32_t GUI_ATTR_COLOR_OFFSET = 16;
/// Byte offset of `color2` (uint), location 3.
inline constexpr uint32_t GUI_ATTR_COLOR2_OFFSET = 20;
/// Byte offset of `radii` (float4), location 4.
inline constexpr uint32_t GUI_ATTR_RADII_OFFSET = 24;
/// Byte offset of `border` (float4), location 5.
inline constexpr uint32_t GUI_ATTR_BORDER_OFFSET = 40;
/// Byte offset of `flags` (uint), location 6.
inline constexpr uint32_t GUI_ATTR_FLAGS_OFFSET = 56;
/// Byte offset of `rect_w` / `rect_h` (float2), location 7.
inline constexpr uint32_t GUI_ATTR_RECT_WH_OFFSET = 60;
/// Byte offset of `param` (float), location 8.
inline constexpr uint32_t GUI_ATTR_PARAM_OFFSET = 68;

}  // namespace eng::gui
