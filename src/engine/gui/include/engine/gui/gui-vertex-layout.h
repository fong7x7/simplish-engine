#pragma once

/// @file gui-vertex-layout.h
/// @brief Vertex attribute layout constants for GUI rendering.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <cstdint>

namespace eng::gui {

/// Number of vertex attributes in the GUI vertex format.
inline constexpr uint32_t GUI_VERTEX_ATTRIBUTE_COUNT = 7;

/// Byte stride between consecutive GUI vertices.
inline constexpr uint32_t GUI_VERTEX_STRIDE = 40;

/// Byte offset of the position attribute in GuiVertex.
inline constexpr uint32_t GUI_ATTR_POS_OFFSET = 0;

/// Byte offset of the UV attribute in GuiVertex.
inline constexpr uint32_t GUI_ATTR_UV_OFFSET = 8;

/// Byte offset of the color attribute in GuiVertex.
inline constexpr uint32_t GUI_ATTR_COLOR_OFFSET = 16;

/// Byte offset of the corner_radius attribute in GuiVertex.
inline constexpr uint32_t GUI_ATTR_CORNER_RADIUS_OFFSET = 20;

/// Byte offset of the border_width attribute in GuiVertex.
inline constexpr uint32_t GUI_ATTR_BORDER_WIDTH_OFFSET = 24;

/// Byte offset of the flags attribute in GuiVertex.
inline constexpr uint32_t GUI_ATTR_FLAGS_OFFSET = 28;

/// Byte offset of `rect_w` / `rect_h` (two floats) in GuiVertex.
inline constexpr uint32_t GUI_ATTR_RECT_WH_OFFSET = 32;

}  // namespace eng::gui
