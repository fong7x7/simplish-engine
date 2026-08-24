#pragma once

#ifdef ENGINE_RENDERER_METAL

// Design Summary -- Metal RHI Device
// Technical Approach:
// docs/technical-approaches/engine/rendering/metal-rhi-backend.md
//
// Behaviours:
//   - Implement RhiDevice interface using Apple Metal API
//   - Manage GPU resources via opaque handle-to-MTL* mapping
//   - Leverage unified memory on Apple Silicon (MTLStorageModeShared)
//   - Fall back to managed mode on discrete AMD/Intel GPUs
//   - Present via CAMetalLayer and CAMetalDrawable
//   - Support compute shaders and optional ray tracing (Apple GPU family 6+)
//
// Edge Cases:
//   - MTLCreateSystemDefaultDevice() returns nil: create() returns nullopt
//   - nextDrawable times out: beginFrame() returns false
//   - Resource allocation failure: returns RHI_*_INVALID handle
//   - Shader compilation failure: returns RHI_SHADER_INVALID
//   - Pipeline creation failure: returns RHI_PIPELINE_INVALID
//
// Invariants:
//   - No Metal/Objective-C headers included here (pImpl isolation)
//   - All Metal/ObjC state is behind Impl in the .mm file
//   - Same RhiDevice interface as all other backends
//   - All public methods are main-thread-only
//
// Integration Points:
//   - RhiDevice interface: MetalRhiDevice is the macOS concrete implementation
//   - RhiDeviceFactory::create() maps RenderConfig → MetalRhiConfig when METAL
//   - PresentationContext: owns the RhiDevice instance

#include "metal-rhi-config.h"

#include <engine/render/rhi-device.h>
#include <memory>
#include <optional>

namespace eng {
/// macOS Metal implementation of the RhiDevice interface.
/// Created via static factory; returns nullopt if Metal is unavailable.
/// All methods are main-thread-only.
///
/// Internal Metal state (MTLDevice, MTLCommandQueue, CAMetalLayer,
/// resource maps) is managed in the .mm file and not exposed here.
class MetalRhiDevice {
public:
  /// Create and initialise the Metal backend. Returns nullopt if
  /// Metal device creation fails or Metal is not available.
  /// Main thread only.
  static std::optional<std::unique_ptr<RhiDevice>>
  create(const MetalRhiConfig& config);

  ~MetalRhiDevice();

  MetalRhiDevice(MetalRhiDevice&& other) noexcept;
  MetalRhiDevice& operator=(MetalRhiDevice&& other) noexcept;

  MetalRhiDevice(const MetalRhiDevice&) = delete;
  MetalRhiDevice& operator=(const MetalRhiDevice&) = delete;

  /// Returns whether the device has unified CPU/GPU memory (Apple Silicon).
  bool hasUnifiedMemory() const;

private:
  MetalRhiDevice();

  struct Impl;
  /// Opaque implementation holding Metal/Objective-C state.
  std::unique_ptr<Impl> impl_;
};

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
