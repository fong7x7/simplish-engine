#include <catch2/catch_test_macros.hpp>

// VulkanCommandList tests require the Vulkan SDK.
// Guard with ENGINE_HAS_VULKAN so the file compiles without the SDK.
#ifdef ENGINE_RENDERER_VULKAN

#include "../../../render/backends/vulkan/src/vulkan-command-list.h"

#include <engine/render/rhi-types.h>

using namespace eng;
using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §4.2
//   — VulkanCommandList wraps VkCommandBuffer

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: nativeCommandBuffer returns wrapped handle",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §4.2
  //   — Wraps a borrowed VkCommandBuffer
  FAIL("Not implemented — VulkanCommandList::nativeCommandBuffer()");
}

// ---------------------------------------------------------------------------
// Recording lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: begin-end recording cycle",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Record commands between begin()/end() calls
  FAIL("Not implemented — VulkanCommandList::begin/end()");
}

// ---------------------------------------------------------------------------
// Render pass
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: beginRenderPass-endRenderPass pair",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Render pass begin/end for framebuffer targeting
  FAIL("Not implemented — VulkanCommandList::beginRenderPass/endRenderPass()");
}

// ---------------------------------------------------------------------------
// Pipeline binding
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: bindPipeline sets active pipeline",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Pipeline must be bound before draw/dispatch
  FAIL("Not implemented — VulkanCommandList::bindPipeline()");
}

// ---------------------------------------------------------------------------
// Resource binding
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: bindVertexBuffer binds at offset",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Bind vertex buffer at slot 0
  FAIL("Not implemented — VulkanCommandList::bindVertexBuffer()");
}

TEST_CASE("VulkanCommandList: bindIndexBuffer with Uint16 and Uint32",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Bind index buffer (u16 or u32)
  FAIL("Not implemented — VulkanCommandList::bindIndexBuffer()");
}

TEST_CASE("VulkanCommandList: bindDescriptorSet at index",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Bind descriptor set at index (forward stub)
  FAIL("Not implemented — VulkanCommandList::bindDescriptorSet()");
}

// ---------------------------------------------------------------------------
// Viewport and scissor
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: setViewport sets rasteriser viewport",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Set viewport
  FAIL("Not implemented — VulkanCommandList::setViewport()");
}

TEST_CASE("VulkanCommandList: setScissor sets clipping rect",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Set scissor rect
  FAIL("Not implemented — VulkanCommandList::setScissor()");
}

// ---------------------------------------------------------------------------
// Draw commands
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: draw issues non-indexed draw",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Non-indexed draw inside render pass
  FAIL("Not implemented — VulkanCommandList::draw()");
}

TEST_CASE("VulkanCommandList: drawIndexed issues indexed draw",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Indexed draw inside render pass
  FAIL("Not implemented — VulkanCommandList::drawIndexed()");
}

// ---------------------------------------------------------------------------
// Compute dispatch
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: dispatch issues compute work",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Compute dispatch outside render pass
  FAIL("Not implemented — VulkanCommandList::dispatch()");
}

// ---------------------------------------------------------------------------
// Copy commands
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: copyBuffer performs buffer-to-buffer copy",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Buffer-to-buffer copy
  FAIL("Not implemented — VulkanCommandList::copyBuffer()");
}

TEST_CASE("VulkanCommandList: copyTextureToBuffer performs readback",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.3
  //   — GPU-to-CPU readback for capture
  FAIL("Not implemented — VulkanCommandList::copyTextureToBuffer()");
}

// ---------------------------------------------------------------------------
// Barriers
// ---------------------------------------------------------------------------

TEST_CASE("VulkanCommandList: textureBarrier transitions layout",
          "[vulkan][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/rhi-interface.md §5.2
  //   — Layout transition
  FAIL("Not implemented — VulkanCommandList::textureBarrier()");
}

#endif  // ENGINE_RENDERER_VULKAN
