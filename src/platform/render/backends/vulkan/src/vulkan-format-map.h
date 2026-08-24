#pragma once

#ifdef ENGINE_RENDERER_VULKAN

#include <engine/render/rhi-types.h>
#include <vulkan/vulkan.h>

namespace eng::render {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// VulkanFormatMap: Compile-time mapping from RhiFormat to VkFormat.
//
// Responsibilities:
// - Convert RHI pixel formats to Vulkan equivalents
// - Convert RHI buffer/texture usage flags to Vulkan usage flags
// - Convert RHI topology and layout enums to Vulkan equivalents
//
// Key Invariants:
// - All functions are constexpr or inline; no runtime state
// - Unmapped formats return VK_FORMAT_UNDEFINED
// - Thread safety: stateless (inherently thread-safe)
// ============================================================================

// Named algorithm: toVkFormat
// Stateless 1:1 mapping from RhiFormat enum values to VkFormat constants.
// No side effects; pure lookup table.
/// Converts an RhiFormat to the corresponding VkFormat.
inline VkFormat toVkFormat(RhiFormat fmt) {
  switch (fmt) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiFormat::R8_UNORM:
      return VK_FORMAT_R8_UNORM;
    case RhiFormat::R_G8_UNORM:
      return VK_FORMAT_R8G8_UNORM;
    case RhiFormat::RGB_A8_UNORM:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case RhiFormat::RGB_A8_SRGB:
      return VK_FORMAT_R8G8B8A8_SRGB;
    case RhiFormat::BGR_A8_UNORM:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case RhiFormat::BGR_A8_SRGB:
      return VK_FORMAT_B8G8R8A8_SRGB;
    case RhiFormat::R16_FLOAT:
      return VK_FORMAT_R16_SFLOAT;
    case RhiFormat::R_G16_FLOAT:
      return VK_FORMAT_R16G16_SFLOAT;
    case RhiFormat::RGB_A16_FLOAT:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case RhiFormat::R32_FLOAT:
      return VK_FORMAT_R32_SFLOAT;
    case RhiFormat::R_G32_FLOAT:
      return VK_FORMAT_R32G32_SFLOAT;
    case RhiFormat::R_G_B32_FLOAT:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case RhiFormat::RGB_A32_FLOAT:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case RhiFormat::D16_UNORM:
      return VK_FORMAT_D16_UNORM;
    case RhiFormat::D24_UNORM_S8_UINT:
      return VK_FORMAT_D24_UNORM_S8_UINT;
    case RhiFormat::D32_FLOAT:
      return VK_FORMAT_D32_SFLOAT;
    case RhiFormat::D32_FLOAT_S8_UINT:
      return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case RhiFormat::B_C7_UNORM:
      return VK_FORMAT_BC7_UNORM_BLOCK;
    case RhiFormat::B_C7_SRGB:
      return VK_FORMAT_BC7_SRGB_BLOCK;
    case RhiFormat::ASTC4X4_UNORM:
      return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case RhiFormat::ASTC4X4_SRGB:
      return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

// Named algorithm: toVkBufferUsage
// Stateless bitmask translation from RhiBufferUsage to VkBufferUsageFlags.
// No side effects; pure flag accumulation.
/// Converts RhiBufferUsage flags to VkBufferUsageFlags.
inline VkBufferUsageFlags toVkBufferUsage(RhiBufferUsage usage) {
  VkBufferUsageFlags flags = 0;
  if (usage & RhiBufferUsage::VERTEX) {
    flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  }
  if (usage & RhiBufferUsage::INDEX) {
    flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  if (usage & RhiBufferUsage::UNIFORM) {
    flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  }
  if (usage & RhiBufferUsage::STORAGE) {
    flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  }
  if (usage & RhiBufferUsage::STAGING) {
    flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  }
  if (usage & RhiBufferUsage::INDIRECT) {
    flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
  }
  return flags;
}

// Named algorithm: toVkImageUsage
// Stateless bitmask translation from RhiTextureUsage to VkImageUsageFlags.
// No side effects; pure flag accumulation.
/// Converts RhiTextureUsage flags to VkImageUsageFlags.
inline VkImageUsageFlags toVkImageUsage(RhiTextureUsage usage) {
  VkImageUsageFlags flags = 0;
  if (usage & RhiTextureUsage::SAMPLED) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if (usage & RhiTextureUsage::STORAGE) {
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if (usage & RhiTextureUsage::RENDER_TARGET) {
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if (usage & RhiTextureUsage::DEPTH_STENCIL) {
    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
  if (usage & RhiTextureUsage::TRANSFER_SRC) {
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if (usage & RhiTextureUsage::TRANSFER_DST) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  return flags;
}

// Named algorithm: toVkImageLayout
// Stateless 1:1 mapping from RhiTextureLayout to VkImageLayout.
// No side effects; pure lookup table.
/// Converts an RhiTextureLayout to a VkImageLayout.
inline VkImageLayout toVkImageLayout(RhiTextureLayout layout) {
  switch (layout) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiTextureLayout::RENDER_TARGET:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case RhiTextureLayout::DEPTH_STENCIL:
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case RhiTextureLayout::SHADER_READ_ONLY:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case RhiTextureLayout::TRANSFER_SRC:
      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case RhiTextureLayout::TRANSFER_DST:
      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case RhiTextureLayout::PRESENT:
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case RhiTextureLayout::GENERAL:
      return VK_IMAGE_LAYOUT_GENERAL;
    case RhiTextureLayout::UNDEFINED:
    default:
      return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

/// Converts an RhiPrimitiveTopology to a VkPrimitiveTopology.
inline VkPrimitiveTopology toVkTopology(RhiPrimitiveTopology topo) {
  switch (topo) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiPrimitiveTopology::TRIANGLE_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case RhiPrimitiveTopology::LINE_LIST:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case RhiPrimitiveTopology::POINT_LIST:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    default:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  }
}

/// Converts an RhiLoadOp to a VkAttachmentLoadOp.
inline VkAttachmentLoadOp toVkLoadOp(RhiLoadOp op) {
  switch (op) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiLoadOp::LOAD:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case RhiLoadOp::CLEAR:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    default:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }
}

/// Converts an RhiIndexType to a VkIndexType.
inline VkIndexType toVkIndexType(RhiIndexType type) {
  switch (type) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiIndexType::UINT32:
      return VK_INDEX_TYPE_UINT32;
    default:
      return VK_INDEX_TYPE_UINT16;
  }
}

/// Converts an RhiShaderStage to a VkShaderStageFlagBits.
inline VkShaderStageFlagBits toVkShaderStage(RhiShaderStage stage) {
  switch (stage) {
    // NOLINTNEXTLINE(bugprone-branch-clone) — FP without SDK headers
    case RhiShaderStage::FRAGMENT:
      return VK_SHADER_STAGE_FRAGMENT_BIT;
    case RhiShaderStage::COMPUTE:
      return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
      return VK_SHADER_STAGE_VERTEX_BIT;
  }
}

// Named algorithm: pure lookup table, no side effects.
// Returns bytes per texel for an RhiFormat (0 for compressed/unknown).
inline uint32_t bytesPerTexel(RhiFormat fmt) {
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

#endif  // ENGINE_RENDERER_VULKAN
