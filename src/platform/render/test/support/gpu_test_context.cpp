#include "gpu_test_context.h"

#include <SDL3/SDL_init.h>
#include <cstring>
#include <engine/render/render-config.h>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-device-factory.h>
#include <engine/render/rhi-texture-desc.h>
#include <utility>

namespace eng::test {

namespace {

  /// Bytes in one readback of a test target.
  constexpr uint64_t GPU_TEST_TARGET_BYTES =
      static_cast<uint64_t>(GPU_TEST_SIZE) * GPU_TEST_SIZE * 4;

  /// SDL hands a Vulkan surface only to a window made for Vulkan; Metal
  /// and DX12 take any window.
  SDL_WindowFlags backendWindowFlags() {
    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN;
#ifdef ENGINE_RENDERER_VULKAN
    flags |= SDL_WINDOW_VULKAN;
#endif
    return flags;
  }

  eng::RenderConfig windowConfig(SDL_Window* window) {
    eng::RenderConfig cfg;
    cfg.backbuffer_width = GPU_TEST_SIZE;
    cfg.backbuffer_height = GPU_TEST_SIZE;
    cfg.vsync = false;
    cfg.native_window = window;
    return cfg;
  }

  RhiBufferHandle createReadback(RhiDevice& device) {
    RhiBufferDesc desc{};
    desc.size = GPU_TEST_TARGET_BYTES;
    desc.usage = RhiBufferUsage::STAGING;
    desc.host_visible = true;
    desc.debug_name = "test_readback";
    return device.createBuffer(desc);
  }

  std::vector<uint8_t> readBuffer(RhiDevice& device, RhiBufferHandle buffer) {
    std::vector<uint8_t> texels(GPU_TEST_TARGET_BYTES);
    const void* mapped = device.mapBuffer(buffer);
    if (mapped == nullptr) {
      return {};
    }
    std::memcpy(texels.data(), mapped, texels.size());
    device.unmapBuffer(buffer);
    return texels;
  }

}  // namespace

GpuTestContext::GpuTestContext() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    return;
  }
  window_ = SDL_CreateWindow("simplish gpu test", GPU_TEST_SIZE, GPU_TEST_SIZE,
                             backendWindowFlags());
  if (window_ == nullptr) {
    return;
  }
  auto created = eng::render::RhiDeviceFactory::create(windowConfig(window_));
  if (created.has_value()) {
    device_ = std::move(*created);
  }
}

GpuTestContext::~GpuTestContext() {
  device_.reset();
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
  SDL_Quit();
}

RhiDevice* GpuTestContext::device() const {
  return device_.get();
}

RhiTextureHandle GpuTestContext::createColorTarget() const {
  RhiTextureDesc desc{};
  desc.width = GPU_TEST_SIZE;
  desc.height = GPU_TEST_SIZE;
  desc.format = RhiFormat::BGR_A8_SRGB;
  desc.usage = RhiTextureUsage::RENDER_TARGET | RhiTextureUsage::SAMPLED;
  desc.debug_name = "test_target";
  return device_->createTexture(desc);
}

std::vector<uint8_t> GpuTestContext::renderAndRead(
    RhiTextureHandle target,
    const std::function<void(RhiCommandList&)>& record) const {
  const RhiBufferHandle readback = createReadback(*device_);
  if (!device_->beginFrame()) {
    return {};
  }
  auto cmd = device_->createCommandList();
  cmd->begin();
  record(*cmd);
  cmd->copyTextureToBuffer(target, readback);
  cmd->end();
  device_->submit(*cmd);
  device_->endFrame();
  static_cast<void>(device_->present());
  device_->waitIdle();
  std::vector<uint8_t> texels = readBuffer(*device_, readback);
  device_->destroyBuffer(readback);
  return texels;
}

}  // namespace eng::test
