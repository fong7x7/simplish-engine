#pragma once

#include "rhi-ray-tracing.h"
#include "rhi-texture-update-2d.h"
#include "rhi-types.h"

#include <engine/render/rhi-command-list.h>
#include <memory>
#include <optional>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// RhiDevice: Backend-agnostic GPU device abstraction.
//
// Responsibilities:
// - Select and initialize the platform GPU backend (Vulkan, DX12, OpenGL, GNM)
// - Create and destroy GPU resources (buffers, textures, shaders, pipelines)
// - Manage swap chain (beginFrame / present)
// - Provide command list creation for GPU work recording
// - Provide screenshot capture API (framebuffer readback + encode)
// - Expose optional ray tracing extension interface
//
// Key Invariants:
// - Single RhiDevice per engine instance (owned by PresentationContext)
// - nullptr in HEADLESS mode (dedicated server)
// - Construct via eng::RhiDeviceFactory::create (engine/render; backends linked
// by CMake)
// - Resource handles are opaque uint64_t; 0 = invalid sentinel
// - destroy*() is safe to call with invalid handles (no-op)
// - No exceptions; all fallible ops return optional/bool/sentinel
// - All public methods are main-thread-only (M0; async deferred to later
// milestone)
//
// Threading:
// - Constructor, destructor, and all methods called from main thread
// - GPU execution is async; beginFrame() synchronizes via fence
// - Capture blocks until GPU readback completes
// ============================================================================

class RhiDevice {
public:
  virtual ~RhiDevice() = default;

  // --- Identification ---
  virtual RhiBackend backend() const = 0;
  virtual const RhiDeviceCapabilities& capabilities() const = 0;

  // --- Resource lifecycle ---
  virtual RhiBufferHandle createBuffer(const RhiBufferDesc& desc) = 0;
  virtual void destroyBuffer(RhiBufferHandle handle) = 0;
  virtual void* mapBuffer(RhiBufferHandle handle) = 0;
  virtual void unmapBuffer(RhiBufferHandle handle) = 0;

  virtual RhiTextureHandle createTexture(const RhiTextureDesc& desc) = 0;
  virtual void destroyTexture(RhiTextureHandle handle) = 0;

  /// Upload a 2D subregion from CPU memory. Default: unsupported (`false`).
  virtual bool updateTexture2D(RhiTextureHandle handle,
                               const RhiTextureUpdate2D& update);

  virtual RhiShaderHandle createShader(const RhiShaderDesc& desc) = 0;
  virtual void destroyShader(RhiShaderHandle handle) = 0;

  virtual RhiPipelineHandle
  createGraphicsPipeline(const RhiGraphicsPipelineDesc& desc) = 0;
  virtual RhiPipelineHandle
  createComputePipeline(const RhiComputePipelineDesc& desc) = 0;
  virtual void destroyPipeline(RhiPipelineHandle handle) = 0;

  /// Optional retained-mode GUI quad pipeline (rounded rects, borders, atlas
  /// text). Backends that support it set `out_pipeline` and return `true`;
  /// default implementation leaves `out_pipeline` unchanged and returns
  /// `false`.
  virtual bool tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline);

  /// Optional static-mesh pipeline: position and normal vertices, a
  /// world-to-clip matrix in vertex stage bytes at slot 1, depth-tested and
  /// depth-written against a `D32_FLOAT` target.
  ///
  /// Like the GUI pipeline this is a backend builtin rather than something
  /// assembled through `createShader`, because that path takes compiled
  /// bytecode and the project has no shader build step yet. Backends without
  /// one return `false` and meshes simply do not draw there.
  virtual bool tryCreateMeshPipeline(RhiPipelineHandle& out_pipeline);

  /// Optional skinned-mesh pipeline: the static mesh pipeline's bindings and
  /// fragment shader, with a vertex stage that reads `SkinnedMeshVertex`
  /// records and blends each vertex by up to four joints from a
  /// `SkinPalette` in vertex stage bytes at slot 2. A builtin for the same
  /// reason the mesh pipeline is. Backends without one return `false` and
  /// skinned meshes do not draw there.
  virtual bool tryCreateSkinnedMeshPipeline(RhiPipelineHandle& out_pipeline);

  /// Optional outline pipeline for meshes: a full-screen triangle, drawn
  /// with no vertex buffer, that reads the scene's `D32_FLOAT` depth at
  /// fragment texture slot 0 and its parameters from fragment stage bytes at
  /// slot 0, and blends an outline over the colour target wherever the depth
  /// bends sharply. A builtin for the same reason the mesh pipeline is.
  /// Backends without one return `false` and meshes draw unoutlined.
  virtual bool tryCreateMeshOutlinePipeline(RhiPipelineHandle& out_pipeline);

  // --- Swap chain ---
  virtual RhiTextureHandle
  backbufferTexture() const = 0;  // Current frame's backbuffer
  virtual uint32_t backbufferWidth() const = 0;
  virtual uint32_t backbufferHeight() const = 0;

  /// Update drawable / swapchain pixel size after the platform window resizes.
  virtual void resizeSwapchain(uint32_t width, uint32_t height);

  // --- Frame management ---
  virtual bool beginFrame() = 0;  // Acquire swapchain; false = fatal
  virtual void endFrame() = 0;    // Finalize frame recording
  virtual void submit(RhiCommandList& cmd) = 0;
  virtual bool present() = 0;  // Swap; false = swapchain lost

  // --- Command list creation ---
  virtual std::unique_ptr<RhiCommandList> createCommandList() = 0;

  // --- Capture API ---
  virtual std::optional<RhiCaptureResult>
  captureFramebuffer(const RhiCaptureRequest& request) = 0;
  virtual bool captureToFile(const RhiCaptureRequest& request,
                             std::string_view path) = 0;

  // --- Optional extensions ---
  virtual IRhiRayTracing* rayTracing() = 0;  // nullptr if unsupported

  // --- Synchronization ---
  virtual void waitIdle() = 0;  // Drain GPU; call before shutdown or resize

  // Prevent copying
  RhiDevice(const RhiDevice&) = delete;
  RhiDevice& operator=(const RhiDevice&) = delete;

protected:
  RhiDevice() = default;
};

inline bool RhiDevice::updateTexture2D(RhiTextureHandle /*handle*/,
                                       const RhiTextureUpdate2D& /*update*/) {
  return false;
}

inline void RhiDevice::resizeSwapchain(uint32_t /*width*/,
                                       uint32_t /*height*/) {}

inline bool
RhiDevice::tryCreateGuiPipeline(RhiPipelineHandle& /*out_pipeline*/) {
  return false;
}

inline bool
RhiDevice::tryCreateMeshPipeline(RhiPipelineHandle& /*out_pipeline*/) {
  return false;
}

inline bool
RhiDevice::tryCreateSkinnedMeshPipeline(RhiPipelineHandle& /*out_pipeline*/) {
  return false;
}

inline bool
RhiDevice::tryCreateMeshOutlinePipeline(RhiPipelineHandle& /*out_pipeline*/) {
  return false;
}

}  // namespace eng
