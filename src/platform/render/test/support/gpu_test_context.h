#pragma once

/// @file gpu_test_context.h
/// @brief A hidden window with the build's GPU backend on it, for tests that
/// need a real device, and a helper that renders one frame and reads it.
/// @par Threading Main-thread-only, like the device.

#include <SDL3/SDL_video.h>
#include <cstdint>
#include <engine/render/rhi-command-list.h>
#include <engine/render/rhi-device.h>
#include <functional>
#include <memory>
#include <vector>

namespace eng::test {

/// Side, in pixels, of the window and of every offscreen target the tests
/// draw into.
inline constexpr uint32_t GPU_TEST_SIZE = 64;

/// Owns SDL's video subsystem, a hidden window created for the backend the
/// build selected, and a device on it from `RhiDeviceFactory`. `device()`
/// is null on a machine without a driver for that backend, which a test
/// turns into a skip rather than a failure (PLT-RHI-5). Built only with a
/// backend that can draw: Metal, Vulkan or DX12.
class GpuTestContext {
public:
  GpuTestContext();
  ~GpuTestContext();
  GpuTestContext(const GpuTestContext&) = delete;
  GpuTestContext& operator=(const GpuTestContext&) = delete;

  /// The device, or null when none could be created.
  RhiDevice* device() const;

  /// An offscreen colour target in the swapchain's format, which is the one
  /// the built-in pipelines are built for.
  RhiTextureHandle createColorTarget() const;

  /// Begin a frame, record `record` into it, copy `target` out, present,
  /// and wait: the target's texels, four bytes each, row 0 at the top.
  std::vector<uint8_t>
  renderAndRead(RhiTextureHandle target,
                const std::function<void(RhiCommandList&)>& record) const;

private:
  /// The hidden window the device presents to.
  SDL_Window* window_ = nullptr;
  /// The device under test.
  std::unique_ptr<RhiDevice> device_;
};

}  // namespace eng::test
