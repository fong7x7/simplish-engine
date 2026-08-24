#pragma once

#ifdef ENGINE_RENDERER_DX12

#include <d3d12.h>
#include <dxgi1_6.h>
#include <engine/render/rhi-types.h>

namespace eng::render {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Dx12FormatMap: Compile-time mapping from RhiFormat to DXGI_FORMAT.
//
// Responsibilities:
// - Convert RHI pixel formats to DXGI equivalents
// - Convert RHI buffer/texture usage flags to D3D12 resource flags
// - Convert RHI topology and layout enums to D3D12 equivalents
//
// Key Invariants:
// - All functions are inline; no runtime state
// - Unmapped formats return DXGI_FORMAT_UNKNOWN
// - Thread safety: stateless (inherently thread-safe)
// ============================================================================

// Named algorithm: toDxgiFormat
// Stateless 1:1 mapping from RhiFormat enum values to DXGI_FORMAT constants.
// No side effects; pure lookup table.
/// Converts an RhiFormat to the corresponding DXGI_FORMAT.
inline DXGI_FORMAT toDxgiFormat(RhiFormat fmt) {
  switch (fmt) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiFormat::R8_UNORM:
      return DXGI_FORMAT_R8_UNORM;
    case RhiFormat::R_G8_UNORM:
      return DXGI_FORMAT_R8G8_UNORM;
    case RhiFormat::RGB_A8_UNORM:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    case RhiFormat::RGB_A8_SRGB:
      return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case RhiFormat::BGR_A8_UNORM:
      return DXGI_FORMAT_B8G8R8A8_UNORM;
    case RhiFormat::BGR_A8_SRGB:
      return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case RhiFormat::R16_FLOAT:
      return DXGI_FORMAT_R16_FLOAT;
    case RhiFormat::R_G16_FLOAT:
      return DXGI_FORMAT_R16G16_FLOAT;
    case RhiFormat::RGB_A16_FLOAT:
      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case RhiFormat::R32_FLOAT:
      return DXGI_FORMAT_R32_FLOAT;
    case RhiFormat::R_G32_FLOAT:
      return DXGI_FORMAT_R32G32_FLOAT;
    case RhiFormat::R_G_B32_FLOAT:
      return DXGI_FORMAT_R32G32B32_FLOAT;
    case RhiFormat::RGB_A32_FLOAT:
      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case RhiFormat::D16_UNORM:
      return DXGI_FORMAT_D16_UNORM;
    case RhiFormat::D24_UNORM_S8_UINT:
      return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case RhiFormat::D32_FLOAT:
      return DXGI_FORMAT_D32_FLOAT;
    case RhiFormat::D32_FLOAT_S8_UINT:
      return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    case RhiFormat::B_C7_UNORM:
      return DXGI_FORMAT_BC7_UNORM;
    case RhiFormat::B_C7_SRGB:
      return DXGI_FORMAT_BC7_UNORM_SRGB;
    case RhiFormat::ASTC4X4_UNORM:
    case RhiFormat::ASTC4X4_SRGB:
    default:
      return DXGI_FORMAT_UNKNOWN;  // ASTC not supported on DX12; unknown for
                                   // all others
  }
}

// Named algorithm: toDx12HeapType
// Maps host_visible flag to the appropriate D3D12 heap type.
// No side effects; pure conditional.
/// Returns D3D12_HEAP_TYPE_UPLOAD for host-visible, DEFAULT otherwise.
inline D3D12_HEAP_TYPE toDx12HeapType(bool host_visible) {
  return host_visible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
}

// Named algorithm: toDx12ResourceFlags
// Stateless bitmask translation from RhiTextureUsage to D3D12 resource flags.
// No side effects; pure flag accumulation.
/// Converts RhiTextureUsage flags to D3D12_RESOURCE_FLAGS.
inline D3D12_RESOURCE_FLAGS toDx12ResourceFlags(RhiTextureUsage usage) {
  D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
  if (usage & RhiTextureUsage::RENDER_TARGET) {
    flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  }
  if (usage & RhiTextureUsage::DEPTH_STENCIL) {
    flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  }
  if (usage & RhiTextureUsage::STORAGE) {
    flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  }
  return flags;
}

// Named algorithm: toDx12Topology
// Stateless 1:1 mapping from RhiPrimitiveTopology to D3D12 topology type.
// No side effects; pure lookup table.
/// Converts an RhiPrimitiveTopology to a D3D12_PRIMITIVE_TOPOLOGY_TYPE.
inline D3D12_PRIMITIVE_TOPOLOGY_TYPE
toDx12TopologyType(RhiPrimitiveTopology topo) {
  switch (topo) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiPrimitiveTopology::LINE_LIST:
      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case RhiPrimitiveTopology::POINT_LIST:
      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    default:
      return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  }
}

/// Converts an RhiPrimitiveTopology to a D3D_PRIMITIVE_TOPOLOGY.
inline D3D_PRIMITIVE_TOPOLOGY toDx12Topology(RhiPrimitiveTopology topo) {
  switch (topo) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiPrimitiveTopology::TRIANGLE_STRIP:
      return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case RhiPrimitiveTopology::LINE_LIST:
      return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case RhiPrimitiveTopology::POINT_LIST:
      return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:
      return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  }
}

// Named algorithm: toDx12ResourceState
// Stateless 1:1 mapping from RhiTextureLayout to D3D12 resource states.
// No side effects; pure lookup table.
/// Converts an RhiTextureLayout to a D3D12_RESOURCE_STATES.
inline D3D12_RESOURCE_STATES toDx12ResourceState(RhiTextureLayout layout) {
  switch (layout) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiTextureLayout::RENDER_TARGET:
      return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case RhiTextureLayout::DEPTH_STENCIL:
      return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case RhiTextureLayout::SHADER_READ_ONLY:
      return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    case RhiTextureLayout::TRANSFER_SRC:
      return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case RhiTextureLayout::TRANSFER_DST:
      return D3D12_RESOURCE_STATE_COPY_DEST;
    case RhiTextureLayout::PRESENT:
      return D3D12_RESOURCE_STATE_PRESENT;
    case RhiTextureLayout::GENERAL:
      return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case RhiTextureLayout::UNDEFINED:
    default:
      return D3D12_RESOURCE_STATE_COMMON;
  }
}

/// Converts an RhiIndexType to a DXGI_FORMAT for index buffer binding.
inline DXGI_FORMAT toDxgiIndexFormat(RhiIndexType type) {
  switch (type) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiIndexType::UINT32:
      return DXGI_FORMAT_R32_UINT;
    default:
      return DXGI_FORMAT_R16_UINT;
  }
}

// Named algorithm: pure lookup table, no side effects.
// Returns bytes per texel for an RhiFormat (0 for compressed/unknown).
/// Returns bytes per texel for an RhiFormat (0 for compressed/unknown).
inline uint32_t dx12BytesPerTexel(RhiFormat fmt) {
  switch (fmt) {
    case RhiFormat::R8_UNORM:
      return 1;
    case RhiFormat::R_G8_UNORM:
      return 2;
    case RhiFormat::RGB_A8_UNORM:
    case RhiFormat::RGB_A8_SRGB:
    case RhiFormat::BGR_A8_UNORM:
    case RhiFormat::BGR_A8_SRGB:
      return 4;
    case RhiFormat::R16_FLOAT:
      return 2;
    case RhiFormat::R_G16_FLOAT:
    case RhiFormat::R32_FLOAT:
    case RhiFormat::D32_FLOAT:
      return 4;
    case RhiFormat::RGB_A16_FLOAT:
    case RhiFormat::R_G32_FLOAT:
    case RhiFormat::D32_FLOAT_S8_UINT:
      return 8;
    case RhiFormat::R_G_B32_FLOAT:
      return 12;
    case RhiFormat::RGB_A32_FLOAT:
      return 16;
    case RhiFormat::D16_UNORM:
      return 2;
    case RhiFormat::D24_UNORM_S8_UINT:
      return 4;
    default:
      return 0;
  }
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
