#include <engine/render/rhi-device-factory.h>

#ifdef ENGINE_RENDERER_METAL
#include <engine/render/backends/metal/metal-rhi-device.h>
#elifdef ENGINE_RENDERER_DX12
#include <engine/render/backends/dx12/dx12-device.h>
#elifdef ENGINE_RENDERER_VULKAN
#include <engine/render/backends/vulkan/vulkan-device.h>
#elifdef ENGINE_RENDERER_OPENGL
#include <engine/render/backends/opengl/opengl-device.h>
#else
#include <engine/render/backends/stub/rhi-device-stub.h>
#endif

namespace eng::render {

#ifdef ENGINE_RENDERER_METAL
namespace {

  RhiDeviceOptional metalDeviceFromConfig(const eng::RenderConfig& config) {
    MetalRhiConfig mc{};
    mc.native_window = config.native_window;
    mc.backbuffer_width = config.backbuffer_width;
    mc.backbuffer_height = config.backbuffer_height;
    mc.vsync = config.vsync;
    mc.enable_validation = config.enable_validation;
    mc.enable_ray_tracing = config.enable_ray_tracing;
    return MetalRhiDevice::create(mc);
  }

}  // namespace
#endif

#ifdef ENGINE_RENDERER_OPENGL
namespace {

  RhiDeviceOptional openGlDeviceFromConfig(const eng::RenderConfig& config) {
    auto created = eng::render::OpenGlDevice::tryCreate(config);
    if (!created.has_value()) {
      return std::nullopt;
    }
    return std::move(*created);
  }

}  // namespace
#endif

#ifdef ENGINE_RENDERER_DX12
namespace {

  RhiDeviceOptional dx12DeviceFromConfig(const eng::RenderConfig& config) {
    return eng::render::Dx12Device::create(config);
  }

}  // namespace
#endif

#ifdef ENGINE_RENDERER_VULKAN
namespace {

  RhiDeviceOptional vulkanDeviceFromConfig(const eng::RenderConfig& config) {
    return eng::render::VulkanDevice::create(config);
  }

}  // namespace
#endif

RhiDeviceOptional RhiDeviceFactory::create(const eng::RenderConfig& config) {
#ifdef ENGINE_RENDERER_METAL
  return metalDeviceFromConfig(config);
#elifdef ENGINE_RENDERER_DX12
  return dx12DeviceFromConfig(config);
#elifdef ENGINE_RENDERER_VULKAN
  return vulkanDeviceFromConfig(config);
#elifdef ENGINE_RENDERER_OPENGL
  return openGlDeviceFromConfig(config);
#else
  return std::make_unique<StubRhiDevice>(config);
#endif
}

}  // namespace eng::render
