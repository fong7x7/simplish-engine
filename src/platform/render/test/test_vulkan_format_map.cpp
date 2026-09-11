#include <catch2/catch_test_macros.hpp>

// Format map tests require Vulkan SDK headers.
// Guard with ENGINE_HAS_VULKAN so the file compiles without the SDK.
#ifdef ENGINE_RENDERER_VULKAN

#include "vulkan-format-map.h"

#include <engine/render/rhi-types.h>
#include <vulkan/vulkan.h>

using namespace eng;
using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
//   — vulkan-format-map.h: RhiFormat → VkFormat mapping

// ---------------------------------------------------------------------------
// toVkFormat — colour formats
// ---------------------------------------------------------------------------

TEST_CASE("toVkFormat: R8_UNORM maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Format definitions
  REQUIRE(toVkFormat(RhiFormat::R8_UNORM) == VK_FORMAT_R8_UNORM);
}

TEST_CASE("toVkFormat: RGBA8_SRGB maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Format definitions
  REQUIRE(toVkFormat(RhiFormat::RGB_A8_SRGB) == VK_FORMAT_R8G8B8A8_SRGB);
}

TEST_CASE("toVkFormat: BGRA8_SRGB maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Format definitions
  REQUIRE(toVkFormat(RhiFormat::BGR_A8_SRGB) == VK_FORMAT_B8G8R8A8_SRGB);
}

TEST_CASE("toVkFormat: RGBA16_FLOAT maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Format definitions
  REQUIRE(toVkFormat(RhiFormat::RGB_A16_FLOAT) ==
          VK_FORMAT_R16G16B16A16_SFLOAT);
}

TEST_CASE("toVkFormat: RGBA32_FLOAT maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Format definitions
  REQUIRE(toVkFormat(RhiFormat::RGB_A32_FLOAT) ==
          VK_FORMAT_R32G32B32A32_SFLOAT);
}

// ---------------------------------------------------------------------------
// toVkFormat — depth formats
// ---------------------------------------------------------------------------

TEST_CASE("toVkFormat: D16_UNORM maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth/stencil formats
  REQUIRE(toVkFormat(RhiFormat::D16_UNORM) == VK_FORMAT_D16_UNORM);
}

TEST_CASE("toVkFormat: D32_FLOAT maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth/stencil formats
  REQUIRE(toVkFormat(RhiFormat::D32_FLOAT) == VK_FORMAT_D32_SFLOAT);
}

TEST_CASE("toVkFormat: D32_FLOAT_S8_UINT maps correctly",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth/stencil formats
  REQUIRE(toVkFormat(RhiFormat::D32_FLOAT_S8_UINT) ==
          VK_FORMAT_D32_SFLOAT_S8_UINT);
}

// ---------------------------------------------------------------------------
// toVkFormat — compressed formats
// ---------------------------------------------------------------------------

TEST_CASE("toVkFormat: BC7_UNORM maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Compressed formats
  REQUIRE(toVkFormat(RhiFormat::B_C7_UNORM) == VK_FORMAT_BC7_UNORM_BLOCK);
}

TEST_CASE("toVkFormat: ASTC4X4_SRGB maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Compressed formats
  REQUIRE(toVkFormat(RhiFormat::ASTC4X4_SRGB) == VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
}

// ---------------------------------------------------------------------------
// toVkFormat — edge cases
// ---------------------------------------------------------------------------

TEST_CASE("toVkFormat: UNDEFINED maps to VK_FORMAT_UNDEFINED",
          "[vulkan][format-map]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
  //   — Unmapped formats return VK_FORMAT_UNDEFINED
  REQUIRE(toVkFormat(RhiFormat::UNDEFINED) == VK_FORMAT_UNDEFINED);
}

// ---------------------------------------------------------------------------
// toVkBufferUsage
// ---------------------------------------------------------------------------

TEST_CASE("toVkBufferUsage: VERTEX maps to vertex bit",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Buffer usage flags
  auto flags = toVkBufferUsage(RhiBufferUsage::VERTEX);
  REQUIRE((flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0);
}

TEST_CASE("toVkBufferUsage: combined flags map correctly",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Bitmask composition
  auto combined = RhiBufferUsage::VERTEX | RhiBufferUsage::INDEX;
  auto flags = toVkBufferUsage(combined);
  REQUIRE((flags & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0);
  REQUIRE((flags & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0);
  REQUIRE((flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) == 0);
}

TEST_CASE("toVkBufferUsage: STAGING maps to transfer src",
          "[vulkan][format-map]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.2
  //   — Staging buffers use TRANSFER_SRC
  auto flags = toVkBufferUsage(RhiBufferUsage::STAGING);
  REQUIRE((flags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0);
}

// ---------------------------------------------------------------------------
// toVkImageUsage
// ---------------------------------------------------------------------------

TEST_CASE("toVkImageUsage: SAMPLED maps to sampled bit",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Texture usage flags
  auto flags = toVkImageUsage(RhiTextureUsage::SAMPLED);
  REQUIRE((flags & VK_IMAGE_USAGE_SAMPLED_BIT) != 0);
}

TEST_CASE("toVkImageUsage: RENDER_TARGET maps to color attachment",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Texture usage flags
  auto flags = toVkImageUsage(RhiTextureUsage::RENDER_TARGET);
  REQUIRE((flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0);
}

TEST_CASE("toVkImageUsage: DEPTH_STENCIL maps to depth stencil attachment",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth/stencil targets
  auto flags = toVkImageUsage(RhiTextureUsage::DEPTH_STENCIL);
  REQUIRE((flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);
}

// ---------------------------------------------------------------------------
// toVkImageLayout
// ---------------------------------------------------------------------------

TEST_CASE("toVkImageLayout: PRESENT maps to present src",
          "[vulkan][format-map]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.1
  //   — Frame flow uses PRESENT layout
  REQUIRE(toVkImageLayout(RhiTextureLayout::PRESENT) ==
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

TEST_CASE("toVkImageLayout: UNDEFINED maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Barrier layouts
  REQUIRE(toVkImageLayout(RhiTextureLayout::UNDEFINED) ==
          VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST_CASE("toVkImageLayout: SHADER_READ_ONLY maps correctly",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Barrier layouts
  REQUIRE(toVkImageLayout(RhiTextureLayout::SHADER_READ_ONLY) ==
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_CASE("toVkImageLayout: TRANSFER_SRC maps correctly",
          "[vulkan][format-map]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §6.3
  //   — Capture flow uses TRANSFER_SRC layout
  REQUIRE(toVkImageLayout(RhiTextureLayout::TRANSFER_SRC) ==
          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

// ---------------------------------------------------------------------------
// toVkTopology
// ---------------------------------------------------------------------------

TEST_CASE("toVkTopology: TRIANGLE_LIST maps correctly",
          "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Primitive topology
  REQUIRE(toVkTopology(RhiPrimitiveTopology::TRIANGLE_LIST) ==
          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
}

TEST_CASE("toVkTopology: LINE_LIST maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Primitive topology
  REQUIRE(toVkTopology(RhiPrimitiveTopology::LINE_LIST) ==
          VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
}

TEST_CASE("toVkTopology: POINT_LIST maps correctly", "[vulkan][format-map]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Primitive topology
  REQUIRE(toVkTopology(RhiPrimitiveTopology::POINT_LIST) ==
          VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
}

#endif  // ENGINE_RENDERER_VULKAN
