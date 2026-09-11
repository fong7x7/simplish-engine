#pragma once

/// @file gui-frame-buffers.h
/// @brief One frame's copy of the GUI vertex and index buffers.
/// @par Threading Main thread only, owned by `GuiRendererContext`.

#include <cstdint>

namespace eng {

/// The host-visible vertex and index buffers one frame's GUI geometry is
/// written into and drawn from. `GuiRendererContext` keeps several and
/// rotates, so the CPU never rewrites a pair the GPU may still be reading.
struct GuiFrameBuffers {
  /// GPU vertex buffer handle; 0 before the renderer has a device.
  uint64_t vertex_buffer = 0;
  /// GPU index buffer handle; 0 before the renderer has a device.
  uint64_t index_buffer = 0;
  /// Vertex buffer capacity in vertices.
  uint32_t vertex_capacity = 0;
  /// Index buffer capacity in indices.
  uint32_t index_capacity = 0;
};

}  // namespace eng
