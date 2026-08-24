#pragma once

#include "rhi-core-types.h"

#include <cstdint>

namespace eng {

struct RhiDeviceCapabilities {
  /// Active RHI backend for this device.
  RhiBackend backend = RhiBackend::VULKAN;
  /// Whether hardware ray tracing is supported.
  bool ray_tracing_supported = false;
  /// Whether compute shaders are supported.
  bool compute_supported = false;
  /// Whether indirect draw commands are supported (requires compute).
  bool indirect_draw_supported = false;
  /// Maximum single buffer allocation size in bytes.
  uint64_t max_buffer_size = 0;
  /// Maximum 2D texture dimension in texels.
  uint32_t max_texture_dimension_2d = 0;
  /// Maximum number of simultaneously bound descriptor sets.
  uint32_t max_bound_descriptor_sets = 0;
  /// Human-readable GPU device name.
  const char* device_name = "";
  /// API version string reported by the backend.
  const char* api_version = "";
};

}  // namespace eng
