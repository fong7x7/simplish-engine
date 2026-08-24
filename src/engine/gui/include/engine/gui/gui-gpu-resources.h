#pragma once

/// @file gui-gpu-resources.h
/// @brief GPU resource handles for the GUI rendering pipeline.
/// @par Threading Main-thread-only.

#include <cstdint>

namespace eng::gui {

/// GPU resources for the GUI rendering pipeline.
/// Owned by GuiRendererContext. Created in `GuiRendererContext::init()`,
/// destroyed in `GuiRendererContext::shutdown()`.
/// @thread_safety Immutable value type.
struct GuiGpuResources {
  /// Dynamic vertex buffer (host-visible, re-uploaded each frame).
  uint64_t vertex_buffer{};
  /// Dynamic index buffer (host-visible, re-uploaded each frame).
  uint64_t index_buffer{};
  /// Graphics pipeline for GUI quad rendering.
  uint64_t pipeline{};
  /// Vertex shader handle.
  uint64_t vertex_shader{};
  /// Fragment shader handle.
  uint64_t fragment_shader{};
  /// Current vertex buffer capacity in bytes.
  uint64_t vertex_capacity{};
  /// Current index buffer capacity in bytes.
  uint64_t index_capacity{};
};

}  // namespace eng::gui
