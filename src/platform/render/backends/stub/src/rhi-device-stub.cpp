#include <cstring>
#include <engine/render/backends/stub/rhi-device-stub.h>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-texture-update-2d.h>

namespace eng {
namespace {

  constexpr RhiBackend stubBackend() {
#ifdef ENGINE_RENDERER_STUB
    return RhiBackend::STUB;
#elifdef ENGINE_RENDERER_VULKAN
    return RhiBackend::VULKAN;
#elifdef ENGINE_RENDERER_DX12
    return RhiBackend::D_X12;
#elifdef ENGINE_RENDERER_OPENGL
    return RhiBackend::OPEN_GL;
#elifdef ENGINE_RENDERER_PS5
    return RhiBackend::GNM;
#elifdef ENGINE_RENDERER_METAL
    return RhiBackend::METAL;
#else
#error                                                                         \
    "Stub RHI requires ENGINE_RENDERER_STUB, VULKAN, DX12, OPENGL, PS5, or METAL"
#endif
  }

  uint32_t bytesPerTexel(RhiFormat fmt) {
    switch (fmt) {
      case RhiFormat::UNDEFINED:
      case RhiFormat::B_C7_UNORM:
      case RhiFormat::B_C7_SRGB:
      case RhiFormat::ASTC4X4_UNORM:
      case RhiFormat::ASTC4X4_SRGB:
        return 0;
      case RhiFormat::R8_UNORM:
        return 1;
      case RhiFormat::R_G8_UNORM:
        return 2;
      case RhiFormat::RGB_A8_UNORM:
      case RhiFormat::RGB_A8_SRGB:
      case RhiFormat::BGR_A8_UNORM:
      case RhiFormat::BGR_A8_SRGB:
        return 4;
      case RhiFormat::R16_FLOAT:
        return 2;
      case RhiFormat::R_G16_FLOAT:
        return 4;
      case RhiFormat::RGB_A16_FLOAT:
        return 8;
      case RhiFormat::R32_FLOAT:
        return 4;
      case RhiFormat::R_G32_FLOAT:
        return 8;
      case RhiFormat::R_G_B32_FLOAT:
        return 12;
      case RhiFormat::RGB_A32_FLOAT:
        return 16;
      case RhiFormat::D16_UNORM:
        return 2;
      case RhiFormat::D24_UNORM_S8_UINT:
      case RhiFormat::D32_FLOAT:
        return 4;
      case RhiFormat::D32_FLOAT_S8_UINT:
        return 8;
    }
    return 0;
  }

  class StubRhiCommandList final : public RhiCommandList {
  public:
    void begin() override {}
    void end() override {}
    void beginRenderPass(const RhiRenderPassBeginInfo& /*info*/) override {}
    void endRenderPass() override {}
    void bindPipeline(RhiPipelineHandle /*handle*/) override {}
    void bindVertexBuffer(RhiBufferHandle /*handle*/,
                          uint64_t /*offset*/) override {}
    void bindIndexBuffer(RhiBufferHandle /*handle*/, uint64_t /*offset*/,
                         RhiIndexType /*type*/) override {}
    void bindDescriptorSet(uint32_t /*slot*/,
                           RhiDescriptorSetHandle /*handle*/) override {}
    void setViewport(const RhiViewport& /*vp*/) override {}
    void setScissor(const RhiScissor& /*scissor*/) override {}
    void draw(const RhiDrawParams& /*params*/) override {}
    void drawIndexed(const RhiDrawIndexedParams& /*params*/) override {}
    void dispatch(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*z*/) override {}
    void copyBuffer(const RhiCopyBufferParams& /*params*/) override {}
    void copyTextureToBuffer(RhiTextureHandle /*src*/,
                             RhiBufferHandle /*dst*/) override {}
    void textureBarrier(RhiTextureHandle /*handle*/, RhiTextureLayout /*from*/,
                        RhiTextureLayout /*to*/) override {}
  };

}  // namespace

StubRhiDevice::StubRhiDevice(RenderConfig config) : config_(std::move(config)) {
  caps_.backend = stubBackend();
  CpuTexture bb;
  bb.width = config_.backbuffer_width;
  bb.height = config_.backbuffer_height;
  bb.format = RhiFormat::BGR_A8_UNORM;
  const auto bpp = bytesPerTexel(bb.format);
  bb.pixels.assign(static_cast<size_t>(bb.width) * bb.height * bpp, 0);
  textures_[backbuffer_handle_] = std::move(bb);
}

RhiBackend StubRhiDevice::backend() const {
  return stubBackend();
}

const RhiDeviceCapabilities& StubRhiDevice::capabilities() const {
  return caps_;
}

RhiBufferHandle StubRhiDevice::createBuffer(const RhiBufferDesc& desc) {
  auto h = next_handle_++;
  buffers_.emplace(h, std::vector<uint8_t>(static_cast<size_t>(desc.size), 0));
  return h;
}

void StubRhiDevice::destroyBuffer(RhiBufferHandle handle) {
  buffers_.erase(handle);
}

void* StubRhiDevice::mapBuffer(RhiBufferHandle handle) {
  auto it = buffers_.find(handle);
  if (it == buffers_.end()) {
    return nullptr;
  }
  return it->second.data();
}

void StubRhiDevice::unmapBuffer(RhiBufferHandle /*handle*/) {}

RhiTextureHandle StubRhiDevice::createTexture(const RhiTextureDesc& desc) {
  const auto bpp = bytesPerTexel(desc.format);
  if (bpp == 0 || desc.width == 0 || desc.height == 0) {
    return RHI_TEXTURE_INVALID;
  }
  auto h = next_handle_++;
  const auto sz = static_cast<size_t>(desc.width) * desc.height * bpp;
  CpuTexture ct;
  ct.width = desc.width;
  ct.height = desc.height;
  ct.format = desc.format;
  ct.pixels.assign(sz, 0);
  if (desc.initial_pixels != nullptr && sz > 0) {
    std::memcpy(ct.pixels.data(), desc.initial_pixels, sz);
  }
  textures_[h] = std::move(ct);
  return h;
}

void StubRhiDevice::destroyTexture(RhiTextureHandle handle) {
  textures_.erase(handle);
}

bool StubRhiDevice::updateTexture2D(RhiTextureHandle handle,
                                    const RhiTextureUpdate2D& u) {
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
  const auto bpp = bytesPerTexel(ct.format);
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

RhiShaderHandle StubRhiDevice::createShader(const RhiShaderDesc& /*desc*/) {
  return next_handle_++;
}

void StubRhiDevice::destroyShader(RhiShaderHandle /*handle*/) {}

RhiPipelineHandle
StubRhiDevice::createGraphicsPipeline(const RhiGraphicsPipelineDesc& /*desc*/) {
  return next_handle_++;
}

RhiPipelineHandle
StubRhiDevice::createComputePipeline(const RhiComputePipelineDesc& /*desc*/) {
  return next_handle_++;
}

void StubRhiDevice::destroyPipeline(RhiPipelineHandle /*handle*/) {}

bool StubRhiDevice::tryCreateGuiPipeline(RhiPipelineHandle& /*out_pipeline*/) {
  return false;
}

RhiTextureHandle StubRhiDevice::backbufferTexture() const {
  return backbuffer_handle_;
}

uint32_t StubRhiDevice::backbufferWidth() const {
  return config_.backbuffer_width;
}

uint32_t StubRhiDevice::backbufferHeight() const {
  return config_.backbuffer_height;
}

bool StubRhiDevice::beginFrame() {
  return true;
}

void StubRhiDevice::endFrame() {}

void StubRhiDevice::submit(RhiCommandList& /*cmd_list*/) {}

bool StubRhiDevice::present() {
  return true;
}

std::unique_ptr<RhiCommandList> StubRhiDevice::createCommandList() {
  return std::make_unique<StubRhiCommandList>();
}

std::optional<RhiCaptureResult>
StubRhiDevice::captureFramebuffer(const RhiCaptureRequest& /*request*/) {
  return std::nullopt;
}

bool StubRhiDevice::captureToFile(const RhiCaptureRequest& /*request*/,
                                  std::string_view /*path*/) {
  return false;
}

IRhiRayTracing* StubRhiDevice::rayTracing() {
  return nullptr;
}

void StubRhiDevice::waitIdle() {}

StubRhiDevice::CpuTexture StubRhiDevice::makeBackbufferPixels(uint32_t w,
                                                              uint32_t h) {
  const auto bpp = bytesPerTexel(RhiFormat::BGR_A8_UNORM);
  CpuTexture bb;
  bb.width = w;
  bb.height = h;
  bb.format = RhiFormat::BGR_A8_UNORM;
  bb.pixels.assign(static_cast<size_t>(w) * h * bpp, 0);
  return bb;
}

void StubRhiDevice::resizeSwapchain(uint32_t width, uint32_t height) {
  if (width == 0U || height == 0U) {
    return;
  }
  config_.backbuffer_width = width;
  config_.backbuffer_height = height;
  textures_[backbuffer_handle_] = makeBackbufferPixels(width, height);
}

}  // namespace eng
