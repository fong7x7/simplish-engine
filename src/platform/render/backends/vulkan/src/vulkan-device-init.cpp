#include "vulkan-device-impl.h"

#ifdef ENGINE_RENDERER_VULKAN

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers (pure functions)
// ---------------------------------------------------------------------------

namespace {

  VkPhysicalDevice
  pickBestDevice(const std::vector<VkPhysicalDevice>& devices) {
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables) — FP without vulkan.h
    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    for (auto dev : devices) {
      VkPhysicalDeviceProperties props{};
      vkGetPhysicalDeviceProperties(dev, &props);
      if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return dev;
      }
      if (fallback == VK_NULL_HANDLE) {
        fallback = dev;
      }
    }
    return fallback;
  }

  std::string buildApiVersionString(uint32_t version) {
    return std::to_string(VK_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_VERSION_MINOR(version)) + "." +
           std::to_string(VK_VERSION_PATCH(version));
  }

  /// Whether Vulkan validation layers are requested.
  enum class ValidationEnabled { NO, YES };

  /// Append platform-specific surface extensions.
  void appendPlatformSurfaceExtensions(std::vector<const char*>& exts) {
    exts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef __APPLE__
    exts.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#elifdef _WIN32
    exts.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
    exts.push_back("VK_KHR_xcb_surface");
#endif
  }

  std::vector<const char*>
  requiredInstanceExtensions(ValidationEnabled validation) {
    std::vector<const char*> exts;
    appendPlatformSurfaceExtensions(exts);
    if (validation == ValidationEnabled::YES) {
      exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return exts;
  }

  VkApplicationInfo buildAppInfo() {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Simplish";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "Simplish Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;
    return app_info;
  }

  VkInstanceCreateInfo
  buildInstanceCreateInfo(const VkApplicationInfo& app_info,
                          const std::vector<const char*>& exts) {
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app_info;
    info.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    info.ppEnabledExtensionNames = exts.data();
    return info;
  }

  /// Validation layer name constant.
  constexpr const char* VALIDATION_LAYER = "VK_LAYER_KHRONOS_validation";

  /// Attach validation layers to instance create info if enabled.
  void attachValidationLayer(VkInstanceCreateInfo& info,
                             ValidationEnabled validation) {
    if (validation == ValidationEnabled::YES) {
      info.enabledLayerCount = 1;
      info.ppEnabledLayerNames = &VALIDATION_LAYER;
    }
  }

  /// Create a VkInstance, retrying without validation if layers unavailable.
  bool createInstanceWithRetry(VkInstanceCreateInfo& info,
                               ValidationEnabled validation, VkInstance* out) {
    VkResult result = vkCreateInstance(&info, nullptr, out);
    if (result != VK_SUCCESS && validation == ValidationEnabled::YES) {
      info.enabledLayerCount = 0;
      info.ppEnabledLayerNames = nullptr;
      result = vkCreateInstance(&info, nullptr, out);
    }
    return result == VK_SUCCESS;
  }

  uint32_t findGraphicsQueueFamily(
      const std::vector<VkQueueFamilyProperties>& families) {
    for (uint32_t i = 0; i < families.size(); ++i) {
      if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        return i;
      }
    }
    return 0;
  }

  VkDeviceQueueCreateInfo buildQueueCreateInfo(uint32_t family,
                                               const float* priority) {
    VkDeviceQueueCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    info.queueFamilyIndex = family;
    info.queueCount = 1;
    info.pQueuePriorities = priority;
    return info;
  }

  /// Device extension name constant.
  constexpr const char* SWAPCHAIN_EXTENSION = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

  /// Build a VkDeviceCreateInfo with a single queue and swapchain extension.
  VkDeviceCreateInfo
  buildDeviceCreateInfo(const VkDeviceQueueCreateInfo& queue_info) {
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = &SWAPCHAIN_EXTENSION;
    return info;
  }

  bool createCommandPool(VkDevice device, uint32_t queue_family,
                         VkCommandPool* out) {
    VkCommandPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.queueFamilyIndex = queue_family;
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    return vkCreateCommandPool(device, &info, nullptr, out) == VK_SUCCESS;
  }

  bool allocateCommandBuffer(VkDevice device, VkCommandPool pool,
                             VkCommandBuffer* out) {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    return vkAllocateCommandBuffers(device, &info, out) == VK_SUCCESS;
  }

  /// Create a semaphore with default settings.
  bool createSemaphore(VkDevice device, VkSemaphore* out) {
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    return vkCreateSemaphore(device, &info, nullptr, out) == VK_SUCCESS;
  }

  /// Create a fence in the signaled state.
  bool createSignaledFence(VkDevice device, VkFence* out) {
    VkFenceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateFence(device, &info, nullptr, out) == VK_SUCCESS;
  }

  bool createSyncObjects(VkDevice device, PerFrameData& frame) {
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables) — FP; init by call
    bool ok = createSemaphore(device, &frame.image_available);
    ok = ok && createSemaphore(device, &frame.render_finished);
    ok = ok && createSignaledFence(device, &frame.in_flight_fence);
    return ok;
  }

  /// Initialise a single frame's command pool, command buffer, and sync
  /// objects.
  bool initSingleFrame(VkDevice device, uint32_t queue_family,
                       PerFrameData& frame) {
    if (!createCommandPool(device, queue_family, &frame.command_pool)) {
      return false;
    }
    if (!allocateCommandBuffer(device, frame.command_pool,
                               &frame.command_buffer)) {
      return false;
    }
    return createSyncObjects(device, frame);
  }

  VkSurfaceFormatKHR pickSurfaceFormat(VkPhysicalDevice gpu,
                                       VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data());

    for (const auto& fmt : formats) {
      if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
          fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return fmt;
      }
    }
    return formats.front();
  }

  /// Whether vertical sync is enabled for present mode selection.
  enum class VsyncEnabled { NO, YES };

  /// Query available present modes from the physical device.
  std::vector<VkPresentModeKHR> queryPresentModes(VkPhysicalDevice gpu,
                                                  VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
    std::vector<VkPresentModeKHR> modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count,
                                              modes.data());
    return modes;
  }

  // Named algorithm: pickPresentMode
  // Selects the best Vulkan present mode based on vsync preference.
  // Prefers mailbox (triple-buffered) when vsync is off; falls back to FIFO.
  // No side effects; pure query.
  VkPresentModeKHR pickPresentMode(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                                   VsyncEnabled vsync) {
    if (vsync == VsyncEnabled::YES) {
      return VK_PRESENT_MODE_FIFO_KHR;
    }
    for (auto mode : queryPresentModes(gpu, surface)) {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return mode;
      }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D pickSwapExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w,
                            uint32_t h) {
    if (caps.currentExtent.width != UINT32_MAX) {
      return caps.currentExtent;
    }
    VkExtent2D extent{w, h};
    extent.width =
        std::clamp(w, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height =
        std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
  }

  /// Grouped parameters for swapchain creation.
  struct SwapchainParams {
    /// Target surface.
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    /// Surface capabilities.
    const VkSurfaceCapabilitiesKHR& caps;
    /// Chosen surface format and colour space.
    VkSurfaceFormatKHR fmt = {};
    /// Chosen present mode.
    VkPresentModeKHR mode = {};
    /// Chosen swap extent.
    VkExtent2D extent = {};
  };

  uint32_t pickImageCount(const VkSurfaceCapabilitiesKHR& caps) {
    uint32_t count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && count > caps.maxImageCount) {
      count = caps.maxImageCount;
    }
    return count;
  }

  // Named algorithm: buildSwapchainCreateInfo
  // Stateless assembly of VkSwapchainCreateInfoKHR from pre-selected
  // surface format, present mode, and extent. No side effects; pure struct.
  VkSwapchainCreateInfoKHR buildSwapchainCreateInfo(const SwapchainParams& p) {
    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = p.surface;
    info.minImageCount = pickImageCount(p.caps);
    info.imageFormat = p.fmt.format;
    info.imageColorSpace = p.fmt.colorSpace;
    info.imageExtent = p.extent;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = p.caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = p.mode;
    info.clipped = VK_TRUE;
    return info;
  }

  /// Build a VkImageViewCreateInfo for a 2D color swapchain image.
  VkImageViewCreateInfo buildSwapImageViewInfo(VkImage image, VkFormat format) {
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = format;
    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    info.subresourceRange.levelCount = 1;
    info.subresourceRange.layerCount = 1;
    return info;
  }

  /// Create a VkImageView per swapchain image; returns empty on failure.
  std::vector<VkImageView>
  createSwapchainImageViews(VkDevice device, const std::vector<VkImage>& images,
                            VkFormat format) {
    std::vector<VkImageView> views(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
      auto info = buildSwapImageViewInfo(images[i], format);
      if (vkCreateImageView(device, &info, nullptr, &views[i]) != VK_SUCCESS) {
        return {};
      }
    }
    return views;
  }

  // Named algorithm: findPresentQueueFamily
  // Searches for a Vulkan queue family that supports presentation on the given
  // surface. Prefers the graphics family; falls back to linear scan.
  // No side effects; pure query.
  uint32_t findPresentQueueFamily(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                                  uint32_t family_count,
                                  uint32_t graphics_family) {
    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(gpu, graphics_family, surface,
                                         &supported);
    if (supported == VK_TRUE) {
      return graphics_family;
    }
    for (uint32_t i = 0; i < family_count; ++i) {
      vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supported);
      if (supported == VK_TRUE) {
        return i;
      }
    }
    return graphics_family;
  }

  void destroySyncObjects(VkDevice device, PerFrameData& frame) {
    if (frame.in_flight_fence != VK_NULL_HANDLE) {
      vkDestroyFence(device, frame.in_flight_fence, nullptr);
    }
    if (frame.render_finished != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, frame.render_finished, nullptr);
    }
    if (frame.image_available != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, frame.image_available, nullptr);
    }
  }

  /// Convert render config vsync flag to VsyncEnabled enum.
  VsyncEnabled toVsyncEnabled(const RenderConfig& cfg) {
    return cfg.vsync ? VsyncEnabled::YES : VsyncEnabled::NO;
  }

}  // namespace

// ---------------------------------------------------------------------------
// Impl init helpers
// ---------------------------------------------------------------------------

bool VulkanDevice::Impl::initAll() {
  bool ok = initInstance() && selectPhysicalDevice() && initSurface();
  ok = ok && initLogicalDevice() && initAllocator();
  ok = ok && initSwapchain() && initPerFrameData();
  if (ok) {
    populateCapabilities();
  }
  return ok;
}

bool VulkanDevice::Impl::initInstance() {
  auto app_info = buildAppInfo();
  auto validation =
      config.enable_validation ? ValidationEnabled::YES : ValidationEnabled::NO;
  auto exts = requiredInstanceExtensions(validation);
  auto create_info = buildInstanceCreateInfo(app_info, exts);
  attachValidationLayer(create_info, validation);
  return createInstanceWithRetry(create_info, validation, &instance);
}

bool VulkanDevice::Impl::selectPhysicalDevice() {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance, &count, nullptr);
  if (count == 0) {
    return false;
  }
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(instance, &count, devices.data());
  physical_device = pickBestDevice(devices);
  return physical_device != VK_NULL_HANDLE;
}

void VulkanDevice::Impl::discoverQueueFamilies() {
  uint32_t family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> families(family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count,
                                           families.data());
  graphics_family = findGraphicsQueueFamily(families);
  present_family = findPresentQueueFamily(physical_device, surface,
                                          family_count, graphics_family);
}

bool VulkanDevice::Impl::initLogicalDevice() {
  discoverQueueFamilies();
  float priority = 1.0f;
  auto queue_info = buildQueueCreateInfo(graphics_family, &priority);
  auto dev_info = buildDeviceCreateInfo(queue_info);
  if (vkCreateDevice(physical_device, &dev_info, nullptr, &device) !=
      VK_SUCCESS) {
    return false;
  }
  vkGetDeviceQueue(device, graphics_family, 0, &graphics_queue);
  vkGetDeviceQueue(device, present_family, 0, &present_queue);
  return true;
}

bool VulkanDevice::Impl::initAllocator() {
  VmaAllocatorCreateInfo info{};
  info.physicalDevice = physical_device;
  info.device = device;
  info.instance = instance;
  info.vulkanApiVersion = VK_API_VERSION_1_3;
  return vmaCreateAllocator(&info, &allocator) == VK_SUCCESS;
}

bool VulkanDevice::Impl::initSurface() {
  if (config.native_window == nullptr) {
    return false;
  }
  auto* window = static_cast<SDL_Window*>(config.native_window);
  return SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);
}

bool VulkanDevice::Impl::initSwapchain() {
  if (surface == VK_NULL_HANDLE) {
    return false;
  }
  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);

  auto fmt = pickSurfaceFormat(physical_device, surface);
  auto mode = pickPresentMode(physical_device, surface, toVsyncEnabled(config));
  auto extent =
      pickSwapExtent(caps, config.backbuffer_width, config.backbuffer_height);
  auto info = buildSwapchainCreateInfo({surface, caps, fmt, mode, extent});
  return createSwapchainResources(info, fmt.format, extent);
}

bool VulkanDevice::Impl::createSwapchainResources(
    const VkSwapchainCreateInfoKHR& info, VkFormat fmt, VkExtent2D extent) {
  if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain) != VK_SUCCESS) {
    return false;
  }
  swapchain_format = fmt;
  swapchain_extent = extent;

  uint32_t count = 0;
  vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
  swapchain_images.resize(count);
  vkGetSwapchainImagesKHR(device, swapchain, &count, swapchain_images.data());

  swapchain_views = createSwapchainImageViews(device, swapchain_images, fmt);
  return !swapchain_views.empty();
}

bool VulkanDevice::Impl::initPerFrameData() {
  for (auto& frame : frames) {
    if (!initSingleFrame(device, graphics_family, frame)) {
      return false;
    }
  }
  return true;
}

void VulkanDevice::Impl::populateCapabilities() {
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(physical_device, &props);

  device_name_str = props.deviceName;
  api_version_str = buildApiVersionString(props.apiVersion);

  caps.backend = RhiBackend::VULKAN;
  caps.compute_supported = true;
  caps.max_buffer_size = props.limits.maxStorageBufferRange;
  caps.max_texture_dimension_2d = props.limits.maxImageDimension2D;
  caps.max_bound_descriptor_sets = props.limits.maxBoundDescriptorSets;
  caps.device_name = device_name_str.c_str();
  caps.api_version = api_version_str.c_str();
}

// ---------------------------------------------------------------------------
// Impl destroy helpers
// ---------------------------------------------------------------------------

void VulkanDevice::Impl::destroyPerFrameData() {
  for (auto& frame : frames) {
    destroySyncObjects(device, frame);
    if (frame.command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device, frame.command_pool, nullptr);
    }
  }
}

void VulkanDevice::Impl::destroySwapchain() {
  for (auto view : swapchain_views) {
    vkDestroyImageView(device, view, nullptr);
  }
  if (swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }
}

void VulkanDevice::Impl::teardown() {
  destroyPerFrameData();
  destroySwapchain();
  destroyCoreObjects();
}

void VulkanDevice::Impl::destroyCoreObjects() {
  if (allocator != VK_NULL_HANDLE) {
    vmaDestroyAllocator(allocator);
  }
  if (device != VK_NULL_HANDLE) {
    vkDestroyDevice(device, nullptr);
  }
  if (surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance, surface, nullptr);
  }
  if (instance != VK_NULL_HANDLE) {
    vkDestroyInstance(instance, nullptr);
  }
}

// ---------------------------------------------------------------------------
// Impl utility helpers
// ---------------------------------------------------------------------------

bool VulkanDevice::Impl::acquireNextImage(PerFrameData& frame) {
  VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                          frame.image_available, VK_NULL_HANDLE,
                                          &image_index);
  // F7: retry once on out-of-date, not recursively
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapchain();
    result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                   frame.image_available, VK_NULL_HANDLE,
                                   &image_index);
  }
  return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

bool VulkanDevice::Impl::createImageView(VulkanTexture& tex) {
  VkImageViewCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.image = tex.image;
  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.format = tex.format;
  info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.layerCount = 1;
  return vkCreateImageView(device, &info, nullptr, &tex.view) == VK_SUCCESS;
}

void VulkanDevice::Impl::recreateSwapchain() {
  vkDeviceWaitIdle(device);
  destroySwapchain();
  initSwapchain();
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
