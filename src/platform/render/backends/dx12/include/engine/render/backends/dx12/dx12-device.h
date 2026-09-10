#pragma once

#ifdef ENGINE_RENDERER_DX12

// Design Summary -- DirectX 12 RHI Device
// Technical Approach:
// docs/technical-approaches/engine/rendering/dx12-backend.md
//
// Behaviours:
//   - Implement RhiDevice interface using DirectX 12 (feature level 12_0)
//   - Ship built-in GUI and static-mesh pipelines, compiled from HLSL at
//     device creation, since there is no shader build step yet
//   - Manage GPU resources via generational handle table (opaque uint64_t
//   handles)
//   - All GPU memory allocated through D3D12MemoryAllocator (D3D12MA)
//   - Double-buffered frame lifecycle with per-frame fence synchronization
//   - Present via IDXGISwapChain4 from SDL native window HWND
//   - Support optional DXR 1.1 ray tracing extensions
//
// Edge Cases:
//   - CreateDXGIFactory2 / D3D12CreateDevice fails: create() returns nullopt
//   - No suitable adapter found: create() returns nullopt
//   - Swapchain out-of-date: recreate internally on beginFrame(), return false
//     on present()
//   - Resource allocation failure: returns RHI_*_INVALID handle
//   - Shader bytecode invalid: returns RHI_SHADER_INVALID
//   - Debug layer unavailable: retry without, log warning
//
// Invariants:
//   - No D3D12/DXGI headers included here (pImpl isolation)
//   - All D3D12/D3D12MA state is behind Impl in the .cpp files
//   - Same RhiDevice interface as all other backends
//   - All public methods are main-thread-only
//
// Integration Points:
//   - RhiDevice interface: Dx12Device is the DX12 concrete implementation
//   - RhiDeviceFactory::create() forwards RenderConfig when DX12 selected
//   - PresentationContext: owns the RhiDevice instance

#include <engine/render/render-config.h>
#include <engine/render/rhi-device.h>
#include <memory>
#include <optional>
#include <string_view>

namespace eng::render {

/// DirectX 12 implementation of the RhiDevice interface.
/// Created via static factory; returns nullopt if DX12 is unavailable.
/// All methods are main-thread-only.
///
/// Internal D3D12 state (IDXGIFactory, ID3D12Device, D3D12MA::Allocator,
/// swapchain, resource handle tables) is managed in the .cpp files and not
/// exposed here.
class Dx12Device final : public RhiDevice {
public:
  /// Create and initialise the DX12 backend. Returns nullopt if any stage
  /// of DX12 initialisation fails (factory, adapter, device, allocator,
  /// swapchain, or per-frame data).
  static std::optional<std::unique_ptr<Dx12Device>>
  create(const RenderConfig& config);

  ~Dx12Device() override;

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

  /// Create the built-in GUI quad pipeline. Its HLSL is compiled here at
  /// call time, the way the Metal backend compiles its MSL.
  bool tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline) override;
  /// Create the built-in static-mesh pipeline.
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
  Dx12Device(const Dx12Device&) = delete;
  Dx12Device& operator=(const Dx12Device&) = delete;

  /// Opaque implementation data (all D3D12 types hidden here). The name is
  /// public because the backend's own helpers — the command list and the
  /// texture lookups — take it as a parameter; the definition lives only in
  /// the private dx12-device-impl.h, so nothing outside the backend can use
  /// it.
  // A pImpl is a forward declaration by construction: defining it here
  // would pull d3d12.h into every includer of this header. The invariant
  // checker reads only a same-line suppression.
  struct Impl;  // NOLINT(no-forward-decl)

private:
  /// Private constructor; use create() factory.
  Dx12Device();

  /// Pointer to implementation (PIMPL pattern).
  std::unique_ptr<Impl> impl_;
};

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
