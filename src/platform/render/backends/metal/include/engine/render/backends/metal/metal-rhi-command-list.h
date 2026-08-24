#pragma once

#ifdef ENGINE_RENDERER_METAL

// Design Summary -- Metal RHI Command List
// Technical Approach:
// docs/technical-approaches/engine/rendering/metal-rhi-backend.md
//
// Behaviours:
//   - Implement RhiCommandList interface using Metal command encoders
//   - Record render passes via MTLRenderCommandEncoder
//   - Record compute dispatches via MTLComputeCommandEncoder
//   - Record buffer/texture copies via MTLBlitCommandEncoder
//   - Barrier via Metal automatic resource tracking (explicit only when needed)
//
// Edge Cases:
//   - beginRenderPass called while already in a render pass: assert
//   - dispatch called inside a render pass: assert
//   - draw called outside a render pass: assert
//
// Invariants:
//   - No Metal/Objective-C headers included here (pImpl isolation)
//   - Created via MetalRhiDevice::createCommandList() only
//   - All methods are main-thread-only
//   - Command buffer not reusable after submit until next frame
//
// Integration Points:
//   - MetalRhiDevice: creates and submits these command lists
//   - RhiCommandList: implements the abstract interface

#include <memory>

namespace eng {

// Forward declarations
class RhiCommandList;

/// Internal Metal command list. Created by MetalRhiDevice::createCommandList().
/// Not intended for direct construction by engine/game code.
///
/// Metal state (MTLCommandBuffer, encoders) is behind Impl in the .mm file.
class MetalRhiCommandList {
public:
  ~MetalRhiCommandList();

  MetalRhiCommandList(MetalRhiCommandList&& other) noexcept;
  MetalRhiCommandList& operator=(MetalRhiCommandList&& other) noexcept;

  MetalRhiCommandList(const MetalRhiCommandList&) = delete;
  MetalRhiCommandList& operator=(const MetalRhiCommandList&) = delete;

private:
  friend class MetalRhiDevice;
  MetalRhiCommandList();

  struct Impl;
  /// Opaque implementation holding Metal command buffer and encoder state.
  std::unique_ptr<Impl> impl_;
};

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
