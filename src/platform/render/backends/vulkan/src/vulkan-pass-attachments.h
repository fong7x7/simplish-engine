#pragma once

#ifdef ENGINE_RENDERER_VULKAN

/// @file vulkan-pass-attachments.h
/// @brief What the open render pass has attached.
/// @par Threading Main-thread-only, owned by one command list.

#include <array>
#include <cstdint>
#include <engine/render/rhi-types.h>
#include <vulkan/vulkan.h>

namespace eng::render {

/// Most colour attachments one pass can bind. Vulkan promises at least 4;
/// every desktop device offers 8.
inline constexpr uint32_t VULKAN_MAX_COLOR_TARGETS = 8;

/// The attachments of one render pass, kept from `beginRenderPass` to
/// `endRenderPass` so the pass can hand its targets back afterwards.
struct VulkanPassAttachments {
  /// Colour attachment infos, in the order the pass named its targets.
  std::array<VkRenderingAttachmentInfo, VULKAN_MAX_COLOR_TARGETS> colors{};
  /// The textures behind `colors`.
  std::array<RhiTextureHandle, VULKAN_MAX_COLOR_TARGETS> color_targets{};
  /// How many colour attachments are in use.
  uint32_t color_count = 0;
  /// Depth attachment info, meaningful when `depth_target` is set.
  VkRenderingAttachmentInfo depth{};
  /// The texture behind `depth`, or invalid for a pass without depth.
  RhiTextureHandle depth_target = RHI_TEXTURE_INVALID;
  /// Size of the area the pass renders: its first attachment's.
  VkExtent2D extent{};
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
