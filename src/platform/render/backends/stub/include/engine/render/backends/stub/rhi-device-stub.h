#pragma once

// CPU-backed RHI used when no native Metal/OpenGL backend is the compile-time
// primary renderer. See docs/technical-approaches/engine/rendering/.

#include <cstdint>
#include <engine/render/render-config.h>
#include <engine/render/rhi-device.h>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace eng {

/// Headless/stub device: in-memory buffers/textures for tests and tooling.
class StubRhiDevice final : public RhiDevice {
public:
  explicit StubRhiDevice(RenderConfig config);

  RhiBackend backend() const override;
  const RhiDeviceCapabilities& capabilities() const override;

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

  bool tryCreateGuiPipeline(RhiPipelineHandle& out_pipeline) override;

  RhiTextureHandle backbufferTexture() const override;
  uint32_t backbufferWidth() const override;
  uint32_t backbufferHeight() const override;

  bool beginFrame() override;
  void endFrame() override;
  void submit(RhiCommandList& cmd) override;
  bool present() override;

  std::unique_ptr<RhiCommandList> createCommandList() override;

  std::optional<RhiCaptureResult>
  captureFramebuffer(const RhiCaptureRequest& request) override;
  bool captureToFile(const RhiCaptureRequest& request,
                     std::string_view path) override;

  IRhiRayTracing* rayTracing() override;
  void waitIdle() override;
  void resizeSwapchain(uint32_t width, uint32_t height) override;

private:
  /// CPU-backed 2D texture for stub readback and `updateTexture2D`.
  struct CpuTexture {
    /// Tightly packed row-major texels.
    std::vector<uint8_t> pixels{};
    /// Width in texels.
    uint32_t width = 0;
    /// Height in texels.
    uint32_t height = 0;
    /// Pixel format of `pixels`.
    RhiFormat format = RhiFormat::UNDEFINED;
  };

  /// Render configuration snapshot.
  RenderConfig config_;
  /// Stub device capabilities (defaults).
  RhiDeviceCapabilities caps_{};
  /// Monotonic handle counter.
  uint64_t next_handle_ = 2;
  /// RHI handle for the default backbuffer.
  RhiTextureHandle backbuffer_handle_ = 1;
  /// CPU-backed buffer storage keyed by handle.
  std::unordered_map<RhiBufferHandle, std::vector<uint8_t>> buffers_{};
  /// CPU-backed texture storage keyed by handle.
  std::unordered_map<RhiTextureHandle, CpuTexture> textures_{};

  static CpuTexture makeBackbufferPixels(uint32_t w, uint32_t h);
};

}  // namespace eng
