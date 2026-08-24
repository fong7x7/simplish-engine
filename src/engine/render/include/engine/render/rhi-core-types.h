#pragma once

#include <cstdint>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// RHI core types: enums, handles, and bitmask operators (no descriptors).
//
// `rhi-types.h` includes this file plus per-type descriptor headers. Descriptor
// headers include `rhi-core-types.h` only to avoid include cycles.
// ============================================================================

// --- Backend identification ---

enum class RhiBackend : uint8_t {
  VULKAN,
  D_X12,
  OPEN_GL,
  GNM,
  METAL,
  /// Headless backend: accepts and validates every call, performs no GPU work.
  /// Selected by ENGINE_RENDERER=STUB for determinism and CI runs.
  STUB,
};

// --- Pixel formats ---

enum class RhiFormat : uint32_t {
  UNDEFINED = 0,

  // Color formats
  R8_UNORM,
  R_G8_UNORM,
  RGB_A8_UNORM,
  RGB_A8_SRGB,
  BGR_A8_UNORM,
  BGR_A8_SRGB,
  R16_FLOAT,
  R_G16_FLOAT,
  RGB_A16_FLOAT,
  R32_FLOAT,
  R_G32_FLOAT,
  R_G_B32_FLOAT,
  RGB_A32_FLOAT,

  // Depth / stencil
  D16_UNORM,
  D24_UNORM_S8_UINT,
  D32_FLOAT,
  D32_FLOAT_S8_UINT,

  // Compressed (block)
  B_C7_UNORM,
  B_C7_SRGB,
  ASTC4X4_UNORM,
  ASTC4X4_SRGB,
};

// --- Buffer usage flags (bitmask) ---

enum class RhiBufferUsage : uint32_t {
  VERTEX = 1 << 0,
  INDEX = 1 << 1,
  UNIFORM = 1 << 2,
  STORAGE = 1 << 3,
  STAGING = 1 << 4,
  INDIRECT = 1 << 5,
};

inline RhiBufferUsage operator|(RhiBufferUsage a, RhiBufferUsage b) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) bitmask enum
  return static_cast<RhiBufferUsage>(static_cast<uint32_t>(a) |
                                     static_cast<uint32_t>(b));
}

inline bool operator&(RhiBufferUsage a, RhiBufferUsage b) {
  return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// --- Buffer barrier stage flags (bitmask) ---

enum class RhiBarrierStage : uint32_t {
  COMPUTE_WRITE = 1 << 0,
  INDIRECT_READ = 1 << 1,
  VERTEX_READ = 1 << 2,
  INDEX_READ = 1 << 3,
  TRANSFER_WRITE = 1 << 4,
  TRANSFER_READ = 1 << 5,
  FRAGMENT_READ = 1 << 6,
};

inline RhiBarrierStage operator|(RhiBarrierStage a, RhiBarrierStage b) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) bitmask enum
  return static_cast<RhiBarrierStage>(static_cast<uint32_t>(a) |
                                      static_cast<uint32_t>(b));
}

inline bool operator&(RhiBarrierStage a, RhiBarrierStage b) {
  return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// --- Texture usage flags (bitmask) ---

enum class RhiTextureUsage : uint32_t {
  SAMPLED = 1 << 0,
  STORAGE = 1 << 1,
  RENDER_TARGET = 1 << 2,
  DEPTH_STENCIL = 1 << 3,
  TRANSFER_SRC = 1 << 4,
  TRANSFER_DST = 1 << 5,
};

inline RhiTextureUsage operator|(RhiTextureUsage a, RhiTextureUsage b) {
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) bitmask enum
  return static_cast<RhiTextureUsage>(static_cast<uint32_t>(a) |
                                      static_cast<uint32_t>(b));
}

inline bool operator&(RhiTextureUsage a, RhiTextureUsage b) {
  return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// --- Shader stages (bitmask) ---

enum class RhiShaderStage : uint32_t {
  VERTEX = 1 << 0,
  FRAGMENT = 1 << 1,
  COMPUTE = 1 << 2,
};

// --- Primitive topology ---

enum class RhiPrimitiveTopology : uint8_t {
  TRIANGLE_LIST,
  TRIANGLE_STRIP,
  LINE_LIST,
  POINT_LIST,
};

// --- Render pass operations ---

enum class RhiLoadOp : uint8_t {
  LOAD,
  CLEAR,
  DONT_CARE,
};

enum class RhiStoreOp : uint8_t {
  STORE,
  DONT_CARE,
};

// --- Texture layout (for barriers) ---

enum class RhiTextureLayout : uint8_t {
  UNDEFINED,
  RENDER_TARGET,
  DEPTH_STENCIL,
  SHADER_READ_ONLY,
  TRANSFER_SRC,
  TRANSFER_DST,
  PRESENT,
  GENERAL,
};

// --- Index buffer element type ---

enum class RhiIndexType : uint8_t {
  UINT16,
  UINT32,
};

// --- Capture format ---

enum class RhiCaptureFormat : uint8_t {
  PNG,
  JPEG,
};

// --- Opaque resource handles ---

using RhiBufferHandle = uint64_t;
using RhiTextureHandle = uint64_t;
using RhiPipelineHandle = uint64_t;
using RhiShaderHandle = uint64_t;
using RhiDescriptorSetHandle = uint64_t;

inline constexpr RhiBufferHandle RHI_BUFFER_INVALID = 0;
inline constexpr RhiTextureHandle RHI_TEXTURE_INVALID = 0;
inline constexpr RhiPipelineHandle RHI_PIPELINE_INVALID = 0;
inline constexpr RhiShaderHandle RHI_SHADER_INVALID = 0;
inline constexpr RhiDescriptorSetHandle RHI_DESCRIPTOR_SET_INVALID = 0;

}  // namespace eng
