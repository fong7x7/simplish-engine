#pragma once

#ifdef ENGINE_RENDERER_VULKAN

// Design Summary -- Vulkan RHI Device
// Technical Approach:
// docs/technical-approaches/engine/rendering/vulkan-backend.md
//
// Behaviours:
//   - Implement RhiDevice interface using Vulkan 1.3 + VK_KHR_dynamic_rendering
//   - Manage GPU resources via generational handle table (opaque uint64_t
//   handles)
//   - All GPU memory allocated through VulkanMemoryAllocator (VMA)
//   - Double-buffered frame lifecycle with per-frame sync objects
//   - Present via VkSwapchainKHR from SDL native window surface
//   - Ship the GUI, mesh, skinned mesh and outline pipelines, compiled from
//     GLSL at creation the way the Metal backend compiles its MSL
//   - Bind stage bytes and fragment textures by push descriptor against one
//     shared layout, so the renderers drive it exactly as they drive Metal
//   - Track every image's layout, since the renderers issue no barriers
//   - Defer destroying a resource until the frames that used it retire
//
// Edge Cases:
//   - vkCreateInstance / vkCreateDevice fails: create() returns nullopt
//   - No Vulkan 1.3 device with dynamic rendering, synchronization2 and
//     VK_KHR_push_descriptor: create() returns nullopt
//   - captureFramebuffer() of the back buffer works only between
//     beginFrame() and present(); after present the image belongs to the
//     presentation engine and the capture is nullopt. (Metal reads the
//     drawable after present, which Vulkan does not allow.)
//   - Swapchain out-of-date: recreate internally on beginFrame(), return false
//     on present()
//   - Resource allocation failure: returns RHI_*_INVALID handle
//   - Shader compilation failure: returns RHI_SHADER_INVALID
//   - Validation layers unavailable: retry without, log warning
//
// Invariants:
//   - No Vulkan headers included here (pImpl isolation)
//   - All Vulkan/VMA state is behind Impl in the .cpp files
//   - Same RhiDevice interface as all other backends
//   - All public methods are main-thread-only
//
// Integration Points:
//   - RhiDevice interface: VulkanDevice is the Vulkan concrete implementation
//   - RhiDeviceFactory::create() forwards RenderConfig when VULKAN selected
//   - PresentationContext: owns the RhiDevice instance

#include <engine/render/render-config.h>
#include <engine/render/rhi-device.h>
#include <memory>
#include <optional>
#include <string_view>

namespace eng::render {

/// Vulkan 1.3 implementation of the RhiDevice interface.
/// Created via static factory; returns nullopt if Vulkan is unavailable.
/// All methods are main-thread-only.
///
/// Internal Vulkan state (VkInstance, VkDevice, VmaAllocator, swapchain,
/// resource handle tables) is managed in the .cpp files and not exposed here.
class VulkanDevice final : public RhiDevice {
public:
  /// Create and initialise the Vulkan backend. Returns nullopt if
  /// any stage of Vulkan initialisation fails (instance, device, VMA,
  /// surface, swapchain, or per-frame data).
  static std::optional<std::unique_ptr<VulkanDevice>>
  create(const RenderConfig& config);

  ~VulkanDevice() override;

  // --- Identification ---
  RhiBackend backend() const override;
  const RhiDeviceCapabilities& capabilities() const override;

  // --- Resource lifecycle ---
  RhiBufferHandle createBuffer(const RhiBufferDesc& desc) override;
  void destroyBuffer(RhiBufferHandle handle) override;
  void* mapBuffer(RhiBufferHandle handle) override;
  void unmapBuffer(RhiBufferHandle handle) override;

  RhiTextureHandle createTexture(const RhiTextureDesc& desc) override;
  void destroyTexture(RhiTextureHandle handle) override;
  bool updateTexture2D(RhiTextureHandle handle,
                       const RhiTextureUpdate2D& update) override;

  RhiShaderHandle createShader(const RhiShaderDesc& desc) override;
  void destroyShader(RhiShaderHandle handle) override;

  RhiPipelineHandle
  createGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) override;
  RhiPipelineHandle
  createComputePipeline(const RhiComputePipelineDesc& desc) override;
  void destroyPipeline(RhiPipelineHandle handle) override;

  // --- Built-in pipelines (GLSL compiled at creation, as Metal's MSL is) ---
  bool tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline) override;
  bool tryCreateMeshPipeline(RhiPipelineHandle& out_pipeline) override;
  bool tryCreateSkinnedMeshPipeline(RhiPipelineHandle& out_pipeline) override;
  bool tryCreateMeshOutlinePipeline(RhiPipelineHandle& out_pipeline) override;

  // --- Swap chain ---
  RhiTextureHandle backbufferTexture() const override;
  uint32_t backbufferWidth() const override;
  uint32_t backbufferHeight() const override;
  void resizeSwapchain(uint32_t width, uint32_t height) override;

  // --- Frame management ---
  bool beginFrame() override;
  void endFrame() override;
  void submit(RhiCommandList& cmd) override;
  bool present() override;

  // --- Command list creation ---
  std::unique_ptr<RhiCommandList> createCommandList() override;

  // --- Capture API ---
  std::optional<RhiCaptureResult>
  captureFramebuffer(const RhiCaptureRequest& request) override;
  bool captureToFile(const RhiCaptureRequest& request,
                     std::string_view path) override;

  // --- Optional extensions ---
  IRhiRayTracing* rayTracing() override;

  // --- Synchronization ---
  void waitIdle() override;

  // Non-copyable, non-movable
  VulkanDevice(const VulkanDevice&) = delete;
  VulkanDevice& operator=(const VulkanDevice&) = delete;

  /// Opaque implementation data (all Vulkan types hidden here). The name is
  /// public because the backend's own helpers — the command list above all
  /// — take it as a parameter; the definition lives only in the private
  /// vulkan-device-impl.h, so nothing outside the backend can use it.
  // A pImpl is a forward declaration by construction: defining it here
  // would pull vulkan.h into every includer of this header. The invariant
  // checker reads only a same-line suppression.
  struct Impl;  // NOLINT(no-forward-decl)

private:
  /// Private constructor; use create() factory.
  VulkanDevice();

  /// Pointer to implementation (PIMPL pattern).
  std::unique_ptr<Impl> impl_;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
