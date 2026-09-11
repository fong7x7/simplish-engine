#include "vulkan-device-impl.h"

#ifdef ENGINE_RENDERER_VULKAN

#include <SDL3/SDL_vulkan.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace eng::render {

// ---------------------------------------------------------------------------
// Anonymous-namespace helpers (pure functions)
// ---------------------------------------------------------------------------

namespace {

  /// Bytes of stage bytes one frame can push: a few hundred draws of mesh
  /// uniforms and lights, with room for a skinned character's palettes.
  constexpr VkDeviceSize VULKAN_STAGE_BYTES_CAPACITY = 1024 * 1024;

  /// Size of the zeroed uniform buffer bound at an empty slot; large enough
  /// for the biggest block a built-in shader declares, the joint palette.
  constexpr VkDeviceSize VULKAN_NULL_UNIFORM_BYTES = 4096;

  /// Name of the portability subset extension, which lives in the beta
  /// header; an implementation that advertises it must have it enabled.
  constexpr const char* PORTABILITY_SUBSET_EXTENSION =
      "VK_KHR_portability_subset";

  /// Device extensions the backend cannot run without.
  constexpr std::array<const char*, 2> REQUIRED_DEVICE_EXTENSIONS{
      VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};

  bool supportsVulkan13(VkPhysicalDevice dev) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);
    return props.apiVersion >= VK_API_VERSION_1_3;
  }

  VkPhysicalDevice
  pickBestDevice(const std::vector<VkPhysicalDevice>& devices) {
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables) — FP without vulkan.h
    VkPhysicalDevice fallback = VK_NULL_HANDLE;
    for (auto dev : devices) {
      VkPhysicalDeviceProperties props{};
      vkGetPhysicalDeviceProperties(dev, &props);
      if (!supportsVulkan13(dev)) {
        continue;
      }
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

  /// What SDL needs to make a surface for this platform's windows. On macOS
  /// that list includes portability enumeration, without which the loader
  /// hides MoltenVK.
  std::vector<const char*>
  requiredInstanceExtensions(ValidationEnabled validation) {
    Uint32 count = 0;
    const char* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&count);
    std::vector<const char*> exts;
    if (sdl_exts != nullptr) {
      exts.assign(sdl_exts, sdl_exts + count);
    }
    if (validation == ValidationEnabled::YES) {
      exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return exts;
  }

  bool listsName(const std::vector<const char*>& names, const char* name) {
    return std::ranges::any_of(
        names, [name](const char* n) { return std::strcmp(n, name) == 0; });
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
    if (listsName(exts, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
      info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
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

  std::vector<const char*> supportedDeviceExtensions(VkPhysicalDevice gpu) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> props(count);
    vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, props.data());
    // The names live in `props`, which dies here; keep only the ones the
    // backend asks about, which all have static storage.
    std::vector<const char*> names;
    for (const auto& p : props) {
      if (std::strcmp(p.extensionName, PORTABILITY_SUBSET_EXTENSION) == 0) {
        names.push_back(PORTABILITY_SUBSET_EXTENSION);
      }
      for (const char* wanted : REQUIRED_DEVICE_EXTENSIONS) {
        if (std::strcmp(p.extensionName, wanted) == 0) {
          names.push_back(wanted);
        }
      }
    }
    return names;
  }

  /// The extensions to enable, or empty when a required one is missing.
  std::vector<const char*> pickDeviceExtensions(VkPhysicalDevice gpu) {
    std::vector<const char*> supported = supportedDeviceExtensions(gpu);
    for (const char* name : REQUIRED_DEVICE_EXTENSIONS) {
      if (!listsName(supported, name)) {
        return {};
      }
    }
    return supported;
  }

  /// A Vulkan 1.0–1.3 feature chain. Its members point at one another, so
  /// it is filled in place and never copied.
  struct DeviceFeatures {
    /// Core features, the head of the chain.
    VkPhysicalDeviceFeatures2 core{};
    /// Vulkan 1.2 features.
    VkPhysicalDeviceVulkan12Features v12{};
    /// Vulkan 1.3 features.
    VkPhysicalDeviceVulkan13Features v13{};
  };

  void chainFeatures(DeviceFeatures& f) {
    f.core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    f.v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    f.v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    f.core.pNext = &f.v12;
    f.v12.pNext = &f.v13;
  }

  /// What to enable: dynamic rendering and synchronization2, which every
  /// pass and barrier here uses, and indirect count and wireframe where
  /// the device has them. False when a required one is missing.
  bool pickFeatures(VkPhysicalDevice gpu, DeviceFeatures& out) {
    DeviceFeatures supported;
    chainFeatures(supported);
    vkGetPhysicalDeviceFeatures2(gpu, &supported.core);
    if (supported.v13.dynamicRendering != VK_TRUE ||
        supported.v13.synchronization2 != VK_TRUE) {
      return false;
    }
    chainFeatures(out);
    out.v13.dynamicRendering = VK_TRUE;
    out.v13.synchronization2 = VK_TRUE;
    out.v12.drawIndirectCount = supported.v12.drawIndirectCount;
    out.core.features.fillModeNonSolid =
        supported.core.features.fillModeNonSolid;
    return true;
  }

  VkDeviceCreateInfo
  buildDeviceCreateInfo(const VkDeviceQueueCreateInfo& queue_info,
                        const std::vector<const char*>& exts,
                        const DeviceFeatures& features) {
    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pNext = &features.core;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = static_cast<uint32_t>(exts.size());
    info.ppEnabledExtensionNames = exts.data();
    return info;
  }

  /// One graphics queue from `family`, the given extensions and features.
  VkDevice createLogicalDevice(VkPhysicalDevice gpu, uint32_t family,
                               const std::vector<const char*>& exts,
                               const DeviceFeatures& features) {
    const float priority = 1.0f;
    const auto queue_info = buildQueueCreateInfo(family, &priority);
    const auto info = buildDeviceCreateInfo(queue_info, exts, features);
    VkDevice device = VK_NULL_HANDLE;
    if (vkCreateDevice(gpu, &info, nullptr, &device) != VK_SUCCESS) {
      return VK_NULL_HANDLE;
    }
    return device;
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

  VkSurfaceCapabilitiesKHR querySurfaceCapabilities(VkPhysicalDevice gpu,
                                                    VkSurfaceKHR surface) {
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps);
    return caps;
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

  /// Colour attachment always; transfer source too when the surface allows
  /// it, so the capture API can copy the back buffer out.
  VkImageUsageFlags pickSwapUsage(const VkSurfaceCapabilitiesKHR& caps) {
    return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
           (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
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
    info.imageUsage = pickSwapUsage(p.caps);
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
    if (frame.image_available != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, frame.image_available, nullptr);
    }
  }

  /// Convert render config vsync flag to VsyncEnabled enum.
  VsyncEnabled toVsyncEnabled(const RenderConfig& cfg) {
    return cfg.vsync ? VsyncEnabled::YES : VsyncEnabled::NO;
  }

  /// A mapped, zeroed uniform buffer; its buffer is null on failure.
  VulkanBuffer createZeroedUniform(VmaAllocator allocator) {
    VkBufferCreateInfo buf_info{};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = VULKAN_NULL_UNIFORM_BYTES;
    buf_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                       VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VulkanBuffer buf{};
    VmaAllocationInfo mapped{};
    if (vmaCreateBuffer(allocator, &buf_info, &alloc_info, &buf.buffer,
                        &buf.allocation, &mapped) == VK_SUCCESS) {
      std::memset(mapped.pMappedData, 0, VULKAN_NULL_UNIFORM_BYTES);
      vmaFlushAllocation(allocator, buf.allocation, 0, VK_WHOLE_SIZE);
    }
    return buf;
  }

  /// A texture's image, as a ref that tracks its layout in place.
  VulkanImageRef textureRef(VulkanTexture& tex) {
    VulkanImageRef ref{};
    ref.image = tex.image;
    ref.view = tex.view;
    ref.aspect = tex.aspect;
    ref.format = tex.format;
    ref.extent = {tex.width, tex.height};
    ref.usage = tex.usage;
    ref.layout = &tex.layout;
    return ref;
  }

  void destroyRetiredObject(VkDevice device, VmaAllocator allocator,
                            const VulkanRetiredObject& object) {
    if (object.pipeline != VK_NULL_HANDLE) {
      vkDestroyPipeline(device, object.pipeline, nullptr);
    }
    if (object.view != VK_NULL_HANDLE) {
      vkDestroyImageView(device, object.view, nullptr);
    }
    if (object.image != VK_NULL_HANDLE) {
      vmaDestroyImage(allocator, object.image, object.allocation);
    }
    if (object.buffer != VK_NULL_HANDLE) {
      vmaDestroyBuffer(allocator, object.buffer, object.allocation);
    }
  }

}  // namespace

// ---------------------------------------------------------------------------
// Impl init helpers
// ---------------------------------------------------------------------------

bool VulkanDevice::Impl::initAll() {
  // No window, no surface — and before a Vulkan window exists SDL has no
  // loader to ask for surface extensions, and crashes if asked.
  if (config.native_window == nullptr) {
    return false;
  }
  bool ok = initInstance() && selectPhysicalDevice() && initSurface();
  ok = ok && initLogicalDevice() && initAllocator();
  ok = ok && initSwapchain() && initPerFrameData();
  ok = ok && initSharedLayout() && initUploadResources();
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
  if (physical_device == VK_NULL_HANDLE) {
    return false;
  }
  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(physical_device, &props);
  uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
  return true;
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
  const std::vector<const char*> exts = pickDeviceExtensions(physical_device);
  DeviceFeatures features;
  if (exts.empty() || !pickFeatures(physical_device, features)) {
    return false;
  }
  draw_indirect_count = features.v12.drawIndirectCount == VK_TRUE;
  device =
      createLogicalDevice(physical_device, graphics_family, exts, features);
  if (device == VK_NULL_HANDLE) {
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
  const VkSurfaceCapabilitiesKHR surface_caps =
      querySurfaceCapabilities(physical_device, surface);
  auto fmt = pickSurfaceFormat(physical_device, surface);
  auto mode = pickPresentMode(physical_device, surface, toVsyncEnabled(config));
  auto extent = pickSwapExtent(surface_caps, config.backbuffer_width,
                               config.backbuffer_height);
  auto info =
      buildSwapchainCreateInfo({surface, surface_caps, fmt, mode, extent});
  swapchain_usage = info.imageUsage;
  return createSwapchainResources(info, fmt.format, extent) &&
         createPresentSemaphores();
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
  swapchain_layouts.assign(count, VK_IMAGE_LAYOUT_UNDEFINED);

  swapchain_views = createSwapchainImageViews(device, swapchain_images, fmt);
  return !swapchain_views.empty();
}

bool VulkanDevice::Impl::createPresentSemaphores() {
  present_ready.assign(swapchain_images.size(), VK_NULL_HANDLE);
  for (auto& semaphore : present_ready) {
    if (!createSemaphore(device, &semaphore)) {
      return false;
    }
  }
  return true;
}

bool VulkanDevice::Impl::initPerFrameData() {
  for (auto& frame : frames) {
    if (!initSingleFrame(device, graphics_family, frame)) {
      return false;
    }
    if (!frame.stage_bytes.create(allocator, VULKAN_STAGE_BYTES_CAPACITY,
                                  uniform_alignment)) {
      return false;
    }
  }
  return true;
}

bool VulkanDevice::Impl::initSharedLayout() {
  push_descriptor_set = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
      vkGetDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR"));
  return push_descriptor_set != nullptr &&
         createVulkanSharedLayout(device, shared_layout);
}

bool VulkanDevice::Impl::initUploadResources() {
  if (!createCommandPool(device, graphics_family, &upload_pool)) {
    return false;
  }
  null_uniform = createZeroedUniform(allocator);
  return null_uniform.buffer != VK_NULL_HANDLE;
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
    frame.stage_bytes.destroy(allocator);
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
  for (VkSemaphore semaphore : present_ready) {
    vkDestroySemaphore(device, semaphore, nullptr);
  }
  if (swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
  }
  swapchain = VK_NULL_HANDLE;
  swapchain_views.clear();
  swapchain_images.clear();
  swapchain_layouts.clear();
  present_ready.clear();
}

void VulkanDevice::Impl::destroyResources() {
  releaseRetired(UINT64_MAX);
  pipelines.forEachAlive(
      [this](VulkanPipeline& p) { retire({0, {}, {}, {}, {}, p.pipeline}); });
  shaders.forEachAlive([this](VulkanShader& s) {
    vkDestroyShaderModule(device, s.module, nullptr);
  });
  textures.forEachAlive([this](VulkanTexture& t) {
    retire({0, {}, t.image, t.view, t.allocation, {}});
  });
  buffers.forEachAlive([this](VulkanBuffer& b) {
    retire({0, b.buffer, {}, {}, b.allocation, {}});
  });
  releaseRetired(UINT64_MAX);
}

void VulkanDevice::Impl::destroyBackendObjects() {
  destroyVulkanSharedLayout(device, shared_layout);
  if (upload_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device, upload_pool, nullptr);
  }
  if (null_uniform.buffer != VK_NULL_HANDLE) {
    vmaDestroyBuffer(allocator, null_uniform.buffer, null_uniform.allocation);
  }
}

void VulkanDevice::Impl::teardown() {
  if (device != VK_NULL_HANDLE) {
    destroyResources();
    destroyBackendObjects();
    destroyPerFrameData();
    destroySwapchain();
  }
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
  // A sampled view of a depth-stencil image may name only one aspect; the
  // depth is the one the outline pass reads.
  info.subresourceRange.aspectMask =
      tex.aspect & ~VkImageAspectFlags{VK_IMAGE_ASPECT_STENCIL_BIT};
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.layerCount = 1;
  return vkCreateImageView(device, &info, nullptr, &tex.view) == VK_SUCCESS;
}

void VulkanDevice::Impl::recreateSwapchain() {
  vkDeviceWaitIdle(device);
  destroySwapchain();
  initSwapchain();
}

VulkanImageRef VulkanDevice::Impl::imageRef(RhiTextureHandle handle) {
  // Handles 1..N are the swapchain images; see `backbufferTexture`.
  const auto sc_count = static_cast<RhiTextureHandle>(swapchain_images.size());
  if (handle > 0 && handle <= sc_count) {
    const auto i = static_cast<size_t>(handle - 1);
    return {swapchain_images[i],  swapchain_views[i], VK_IMAGE_ASPECT_COLOR_BIT,
            swapchain_format,     swapchain_extent,   swapchain_usage,
            &swapchain_layouts[i]};
  }
  auto* tex = textures.lookup(handle);
  return tex != nullptr ? textureRef(*tex) : VulkanImageRef{};
}

VkCommandBuffer VulkanDevice::Impl::beginOneShot() {
  VkCommandBuffer cmd = VK_NULL_HANDLE;
  if (!allocateCommandBuffer(device, upload_pool, &cmd)) {
    return VK_NULL_HANDLE;
  }
  VkCommandBufferBeginInfo begin{};
  begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &begin);
  return cmd;
}

void VulkanDevice::Impl::submitOneShot(VkCommandBuffer cmd) {
  vkEndCommandBuffer(cmd);
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  vkQueueSubmit(graphics_queue, 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphics_queue);
  vkFreeCommandBuffers(device, upload_pool, 1, &cmd);
}

void VulkanDevice::Impl::retire(VulkanRetiredObject object) {
  object.frame_serial = frame_serial;
  retired.push_back(object);
}

void VulkanDevice::Impl::releaseRetired(uint64_t completed_serial) {
  auto done = [completed_serial](const VulkanRetiredObject& o) {
    return o.frame_serial <= completed_serial;
  };
  for (const auto& object : retired) {
    if (done(object)) {
      destroyRetiredObject(device, allocator, object);
    }
  }
  std::erase_if(retired, done);
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
