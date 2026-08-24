#pragma once

#ifdef ENGINE_RENDERER_VULKAN

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// VulkanDevice::Impl — PIMPL body for VulkanDevice.
//
// Responsibilities:
// - Hold all Vulkan state (instance, device, allocator, swapchain, queues)
// - Define internal resource structs (VulkanBuffer, VulkanTexture, etc.)
// - Provide init/destroy/utility helpers called by VulkanDevice methods
//
// Key Invariants:
// - This header is internal; never included from public headers
// - All Vulkan types are confined here (pImpl isolation)
// - Thread safety: main-thread-only (same as VulkanDevice)
// ============================================================================

#include "vulkan-buffer-resource.h"
#include "vulkan-frames-in-flight.h"
#include "vulkan-handle-table.h"
#include "vulkan-per-frame-data.h"
#include "vulkan-pipeline-resource.h"
#include "vulkan-shader-resource.h"
#include "vulkan-texture-resource.h"

#include <array>
#include <engine/render/backends/vulkan/vulkan-device.h>
#include <engine/render/render-config.h>
#include <engine/render/rhi-device-capabilities.h>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace eng::render {

struct VulkanDevice::Impl {
  /// Vulkan instance.
  VkInstance instance = VK_NULL_HANDLE;
  /// Debug messenger (only if validation enabled).
  VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
  /// Selected physical device.
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  /// Logical device.
  VkDevice device = VK_NULL_HANDLE;
  /// Graphics queue.
  VkQueue graphics_queue = VK_NULL_HANDLE;
  /// Present queue (may alias graphics_queue).
  VkQueue present_queue = VK_NULL_HANDLE;
  /// Graphics queue family index.
  uint32_t graphics_family = 0;
  /// Present queue family index.
  uint32_t present_family = 0;
  /// VMA allocator for GPU memory.
  VmaAllocator allocator = VK_NULL_HANDLE;
  /// Swapchain.
  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  /// Window surface.
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  /// Swapchain image format.
  VkFormat swapchain_format = VK_FORMAT_UNDEFINED;
  /// Swapchain extent.
  VkExtent2D swapchain_extent{};
  /// Swapchain images.
  std::vector<VkImage> swapchain_images;
  /// Swapchain image views.
  std::vector<VkImageView> swapchain_views;
  /// Index of the current swapchain image.
  uint32_t image_index = 0;
  /// Current frame index (0..FRAMES_IN_FLIGHT-1).
  uint32_t frame_index = 0;
  /// Per-frame sync and command data.
  std::array<PerFrameData, FRAMES_IN_FLIGHT> frames{};
  /// Cached device capabilities.
  RhiDeviceCapabilities caps{};
  /// Device name string (owned for lifetime stability).
  std::string device_name_str;
  /// API version string (owned for lifetime stability).
  std::string api_version_str;
  /// Render config used at creation.
  RenderConfig config{};
  /// Buffer handle table.
  VulkanHandleTable<VulkanBuffer> buffers;
  /// Texture handle table.
  VulkanHandleTable<VulkanTexture> textures;
  /// Shader handle table.
  VulkanHandleTable<VulkanShader> shaders;
  /// Pipeline handle table.
  VulkanHandleTable<VulkanPipeline> pipelines;

  // --- Init helpers ---

  /// Run all initialisation stages in order; returns false on any failure.
  bool initAll();
  /// Discover graphics and present queue family indices.
  void discoverQueueFamilies();
  /// Create the VkInstance with optional validation layers.
  bool initInstance();
  /// Select the best VkPhysicalDevice (prefers discrete GPU).
  bool selectPhysicalDevice();
  /// Create the VkDevice and retrieve queue handles.
  bool initLogicalDevice();
  /// Create the VMA allocator for GPU memory.
  bool initAllocator();
  /// Create the VkSurfaceKHR from the native window handle.
  bool initSurface();
  /// Create or recreate the VkSwapchainKHR and image views.
  bool initSwapchain();
  /// Allocate per-frame command pools, command buffers, and sync objects.
  bool initPerFrameData();
  /// Query physical device properties and fill the capabilities struct.
  void populateCapabilities();
  /// Create swapchain object and associated image views.
  bool createSwapchainResources(const VkSwapchainCreateInfoKHR& info,
                                VkFormat fmt, VkExtent2D extent);

  // --- Destroy helpers ---

  /// Destroy per-frame sync objects and command pools.
  void destroyPerFrameData();
  /// Destroy swapchain image views and the swapchain itself.
  void destroySwapchain();
  /// Destroy allocator, logical device, surface, and instance.
  void destroyCoreObjects();
  /// Full teardown: per-frame data, swapchain, and core objects.
  void teardown();

  // --- Utility helpers ---

  /// Acquire the next swapchain image; retries once on out-of-date.
  bool acquireNextImage(PerFrameData& frame);
  /// Create a VkImageView for a user-created texture.
  bool createImageView(VulkanTexture& tex);
  /// Recreate the swapchain after a resize or out-of-date event.
  void recreateSwapchain();
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
