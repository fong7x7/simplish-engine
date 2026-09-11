#include "vulkan-image-transition.h"

#ifdef ENGINE_RENDERER_VULKAN

#include <array>

namespace eng::render {

namespace {

  /// The stages that touch an image in a given layout, and how.
  struct LayoutSync {
    /// The layout this entry describes.
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    /// Pipeline stages that use an image in this layout.
    VkPipelineStageFlags2 stage = VK_PIPELINE_STAGE_2_NONE;
    /// The accesses those stages make.
    VkAccessFlags2 access = VK_ACCESS_2_NONE;
  };

  // Present comes after the colour output stage on the way in and out, so
  // the barrier chains with the acquire semaphore's wait at that stage.
  constexpr std::array<LayoutSync, 6> LAYOUT_SYNC{{
      {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
       VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
           VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT},
      {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
       VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
           VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
       VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
      {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
       VK_ACCESS_2_SHADER_SAMPLED_READ_BIT},
      {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
       VK_ACCESS_2_TRANSFER_READ_BIT},
      {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT,
       VK_ACCESS_2_TRANSFER_WRITE_BIT},
      {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
       VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE},
  }};

  /// Undefined and general fall through to "everything": an image coming
  /// out of nowhere waits on all prior work, which is also what makes a
  /// first-frame swapchain transition safe against the acquire.
  LayoutSync syncFor(VkImageLayout layout) {
    for (const LayoutSync& entry : LAYOUT_SYNC) {
      if (entry.layout == layout) {
        return entry;
      }
    }
    return {layout, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
  }

  VkImageMemoryBarrier2 buildBarrier(const VulkanImageRef& ref,
                                     VkImageLayout to) {
    const LayoutSync src = syncFor(*ref.layout);
    const LayoutSync dst = syncFor(to);
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = src.stage;
    barrier.srcAccessMask = src.access;
    barrier.dstStageMask = dst.stage;
    barrier.dstAccessMask = dst.access;
    barrier.oldLayout = *ref.layout;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = ref.image;
    barrier.subresourceRange = {ref.aspect, 0, VK_REMAINING_MIP_LEVELS, 0,
                                VK_REMAINING_ARRAY_LAYERS};
    return barrier;
  }

}  // namespace

void transitionVulkanImage(VkCommandBuffer cmd, const VulkanImageRef& ref,
                           VkImageLayout to) {
  if (ref.image == VK_NULL_HANDLE || ref.layout == nullptr ||
      *ref.layout == to) {
    return;
  }
  const VkImageMemoryBarrier2 barrier = buildBarrier(ref, to);
  VkDependencyInfo dep{};
  dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dep.imageMemoryBarrierCount = 1;
  dep.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(cmd, &dep);
  *ref.layout = to;
}

VkImageLayout vulkanRestingLayout(const VulkanImageRef& ref) {
  if ((ref.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0) {
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }
  return ref.layout != nullptr ? *ref.layout : VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
