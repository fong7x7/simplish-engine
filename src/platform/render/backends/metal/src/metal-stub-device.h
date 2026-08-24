#pragma once

#ifdef ENGINE_RENDERER_METAL

// Internal header — CPU-backed stub implementations of RhiDevice and
// RhiCommandList used when Metal is unavailable (no window, headless, CI).
// Extracted from metal-stubs.cpp so the real Metal factory can fall back
// to the stub when native_window is nullptr.

#include "metal-stub-command-list.h"
#include "metal-stub-format-bytes.h"

#include <cstring>
#include <engine/render/backends/metal/metal-rhi-config.h>
#include <engine/render/rhi-capture-request.h>
#include <engine/render/rhi-capture-result.h>
#include <engine/render/rhi-compute-pipeline-desc.h>
#include <engine/render/rhi-device-capabilities.h>
#include <engine/render/rhi-device.h>
#include <engine/render/rhi-texture-update-2d.h>
#include <engine/render/rhi-types.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eng {

/// CPU-backed Metal RHI stub used when no real GPU is available.
/// All resources are tracked in CPU memory via handle maps.
/// Main thread only.
class MetalStubDevice final : public RhiDevice {
public:
  explicit MetalStubDevice(MetalRhiConfig config) : config_(config) {
    CpuTexture bb;
    bb.width = config_.backbuffer_width;
    bb.height = config_.backbuffer_height;
    bb.format = RhiFormat::BGR_A8_UNORM;
    const auto bpp = stubBytesPerTexel(bb.format);
    bb.pixels.assign(static_cast<size_t>(bb.width) * bb.height * bpp, 0);
    textures_[backbuffer_handle_] = std::move(bb);
    populateCapabilities();
  }

  RhiBackend backend() const override { return RhiBackend::METAL; }

  const RhiDeviceCapabilities& capabilities() const override { return caps_; }

  RhiBufferHandle createBuffer(const RhiBufferDesc& desc) override {
    auto h = next_handle_++;
    buffers_.emplace(h,
                     std::vector<uint8_t>(static_cast<size_t>(desc.size), 0));
    return h;
  }

  void destroyBuffer(RhiBufferHandle handle) override {
    buffers_.erase(handle);
  }

  void* mapBuffer(RhiBufferHandle handle) override {
    auto it = buffers_.find(handle);
    if (it == buffers_.end()) {
      return nullptr;
    }
    return it->second.data();
  }

  void unmapBuffer(RhiBufferHandle /*handle*/) override {}

  RhiTextureHandle createTexture(const RhiTextureDesc& desc) override {
    const auto bpp = stubBytesPerTexel(desc.format);
    if (bpp == 0 || desc.width == 0 || desc.height == 0) {
      return RHI_TEXTURE_INVALID;
    }
    auto h = next_handle_++;
    const auto sz = static_cast<size_t>(desc.width) * desc.height * bpp;
    CpuTexture ct;
    ct.width = desc.width;
    ct.height = desc.height;
    ct.format = desc.format;
    ct.pixels.resize(sz, 0);
    if (desc.initial_pixels != nullptr && sz > 0) {
      std::memcpy(ct.pixels.data(), desc.initial_pixels, sz);
    }
    textures_[h] = std::move(ct);
    return h;
  }

  void destroyTexture(RhiTextureHandle handle) override {
    textures_.erase(handle);
  }

  bool updateTexture2D(RhiTextureHandle handle,
                       const RhiTextureUpdate2D& u) override {
    auto it = textures_.find(handle);
    if (it == textures_.end() || u.pixels == nullptr || u.width == 0 ||
        u.height == 0 || u.format == RhiFormat::UNDEFINED) {
      return false;
    }
    auto& ct = it->second;
    if (u.format != ct.format) {
      return false;
    }
    if (u.offset_x + u.width > ct.width || u.offset_y + u.height > ct.height) {
      return false;
    }
    const auto bpp = stubBytesPerTexel(ct.format);
    if (bpp == 0) {
      return false;
    }
    uint32_t row_b = u.bytes_per_row != 0 ? u.bytes_per_row : u.width * bpp;
    if (row_b < u.width * bpp || row_b % bpp != 0) {
      return false;
    }
    const auto* src = static_cast<const uint8_t*>(u.pixels);
    for (uint32_t row = 0; row < u.height; ++row) {
      const uint8_t* srow = src + static_cast<size_t>(row) * row_b;
      const uint32_t dy = u.offset_y + row;
      uint8_t* drow = ct.pixels.data() +
                      (static_cast<size_t>(dy) * ct.width + u.offset_x) * bpp;
      std::memcpy(drow, srow, static_cast<size_t>(u.width) * bpp);
    }
    return true;
  }

  RhiShaderHandle createShader(const RhiShaderDesc& /*desc*/) override {
    return next_handle_++;
  }

  void destroyShader(RhiShaderHandle /*handle*/) override {}

  RhiPipelineHandle
  createGraphicsPipeline(const RhiGraphicsPipelineDesc& /*desc*/) override {
    return next_handle_++;
  }

  RhiPipelineHandle
  createComputePipeline(const RhiComputePipelineDesc& /*desc*/) override {
    return next_handle_++;
  }

  void destroyPipeline(RhiPipelineHandle /*handle*/) override {}

  bool tryCreateGuiPipeline(RhiPipelineHandle& /*out_pipeline*/) override {
    return false;
  }

  RhiTextureHandle backbufferTexture() const override {
    return backbuffer_handle_;
  }

  uint32_t backbufferWidth() const override { return config_.backbuffer_width; }

  uint32_t backbufferHeight() const override {
    return config_.backbuffer_height;
  }

  bool beginFrame() override { return true; }
  void endFrame() override {}
  void submit(RhiCommandList& /*cmd*/) override {}
  bool present() override { return true; }

  std::unique_ptr<RhiCommandList> createCommandList() override {
    return std::make_unique<MetalStubCommandList>();
  }

  std::optional<RhiCaptureResult>
  captureFramebuffer(const RhiCaptureRequest& /*request*/) override {
    return std::nullopt;
  }

  bool captureToFile(const RhiCaptureRequest& /*request*/,
                     std::string_view /*path*/) override {
    return false;
  }

  IRhiRayTracing* rayTracing() override { return nullptr; }

  void waitIdle() override {}

  void resizeSwapchain(uint32_t width, uint32_t height) override;

private:
  /// CPU-backed 2D texture for stub validation paths.
  struct CpuTexture {
    std::vector<uint8_t> pixels{};
    uint32_t width = 0;
    uint32_t height = 0;
    RhiFormat format = RhiFormat::UNDEFINED;
  };

  static CpuTexture makeBackbufferPixels(uint32_t w, uint32_t h) {
    const auto bpp = stubBytesPerTexel(RhiFormat::BGR_A8_UNORM);
    CpuTexture bb;
    bb.width = w;
    bb.height = h;
    bb.format = RhiFormat::BGR_A8_UNORM;
    bb.pixels.assign(static_cast<size_t>(w) * h * bpp, 0);
    return bb;
  }

  /// First valid handle value (0 is invalid sentinel).
  static constexpr uint64_t INITIAL_HANDLE = 1;
  /// Stub device name reported in capabilities.
  static constexpr const char* STUB_DEVICE_NAME = "Metal Stub Device";
  /// Stub API version reported in capabilities.
  static constexpr const char* STUB_API_VERSION = "Metal 3 (stub)";
  /// Maximum 2D texture dimension (typical Apple GPU limit).
  static constexpr uint32_t MAX_TEXTURE_DIM_2D = 16384;
  /// Maximum buffer size (256 MB stub default).
  static constexpr uint64_t MAX_BUFFER_SIZE = 256ULL * 1024 * 1024;
  /// Maximum bound descriptor sets (stub default).
  static constexpr uint32_t MAX_DESCRIPTOR_SETS = 8;

  void populateCapabilities() {
    caps_.backend = RhiBackend::METAL;
    caps_.compute_supported = true;
    caps_.ray_tracing_supported = false;
    caps_.max_buffer_size = MAX_BUFFER_SIZE;
    caps_.max_texture_dimension_2d = MAX_TEXTURE_DIM_2D;
    caps_.max_bound_descriptor_sets = MAX_DESCRIPTOR_SETS;
    caps_.device_name = STUB_DEVICE_NAME;
    caps_.api_version = STUB_API_VERSION;
  }

  /// Startup configuration snapshot.
  MetalRhiConfig config_;
  /// Cached device capabilities.
  RhiDeviceCapabilities caps_{};
  /// Monotonic handle counter.
  uint64_t next_handle_ = INITIAL_HANDLE + 1;
  /// Handle for the simulated backbuffer texture.
  RhiTextureHandle backbuffer_handle_ = INITIAL_HANDLE;
  /// CPU-backed buffer storage keyed by handle.
  std::unordered_map<RhiBufferHandle, std::vector<uint8_t>> buffers_{};
  /// CPU-backed texture storage keyed by handle.
  std::unordered_map<RhiTextureHandle, CpuTexture> textures_{};
};

inline void MetalStubDevice::resizeSwapchain(uint32_t width, uint32_t height) {
  if (width == 0U || height == 0U) {
    return;
  }
  config_.backbuffer_width = width;
  config_.backbuffer_height = height;
  textures_[backbuffer_handle_] = makeBackbufferPixels(width, height);
}

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
