#include <catch2/catch_test_macros.hpp>

// OpenGL type-conversion tests require the OpenGL backend.
// Guard with ENGINE_RENDERER_OPENGL so the file compiles without it.
#ifdef ENGINE_RENDERER_OPENGL

#include <engine/render/backends/opengl/opengl-types.h>

using namespace eng;
using namespace eng::render;

// Req: docs/engine/rendering.md §2 — RHI concepts
// Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3 —
//   Format conversion utilities

// ============================================================================
// GlFormatInfo::fromRhiFormat — colour formats
// ============================================================================

TEST_CASE("GlFormatInfo: RGBA8_UNORM produces valid triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Format definitions
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::RGB_A8_UNORM);
  REQUIRE(info.internal_format != 0);
  REQUIRE(info.format != 0);
  REQUIRE(info.type != 0);
}

TEST_CASE("GlFormatInfo: RGBA8_SRGB produces valid triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — sRGB format
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::RGB_A8_SRGB);
  REQUIRE(info.internal_format != 0);
  REQUIRE(info.format != 0);
  REQUIRE(info.type != 0);
}

TEST_CASE("GlFormatInfo: R8_UNORM produces valid triplet", "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Single channel
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::R8_UNORM);
  REQUIRE(info.internal_format != 0);
  REQUIRE(info.format != 0);
  REQUIRE(info.type != 0);
}

TEST_CASE("GlFormatInfo: RGBA16_FLOAT produces valid triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — HDR format
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::RGB_A16_FLOAT);
  REQUIRE(info.internal_format != 0);
  REQUIRE(info.format != 0);
  REQUIRE(info.type != 0);
}

TEST_CASE("GlFormatInfo: RGBA32_FLOAT produces valid triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Full-precision
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::RGB_A32_FLOAT);
  REQUIRE(info.internal_format != 0);
  REQUIRE(info.format != 0);
  REQUIRE(info.type != 0);
}

TEST_CASE("GlFormatInfo: R32_FLOAT produces valid triplet", "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Float format
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::R32_FLOAT);
  REQUIRE(info.internal_format != 0);
}

TEST_CASE("GlFormatInfo: RG8_UNORM produces valid triplet", "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Two-channel
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::R_G8_UNORM);
  REQUIRE(info.internal_format != 0);
}

TEST_CASE("GlFormatInfo: RG16_FLOAT produces valid triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Two-channel float
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::R_G16_FLOAT);
  REQUIRE(info.internal_format != 0);
}

TEST_CASE("GlFormatInfo: RG32_FLOAT produces valid triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Two-channel float
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::R_G32_FLOAT);
  REQUIRE(info.internal_format != 0);
}

TEST_CASE("GlFormatInfo: R16_FLOAT produces valid triplet", "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Half float
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::R16_FLOAT);
  REQUIRE(info.internal_format != 0);
}

// ============================================================================
// GlFormatInfo::fromRhiFormat — depth/stencil formats
// ============================================================================

TEST_CASE("GlFormatInfo: D16_UNORM produces valid depth triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth formats
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::D16_UNORM);
  REQUIRE(info.internal_format != 0);
}

TEST_CASE("GlFormatInfo: D24_UNORM_S8_UINT produces valid depth triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth/stencil formats
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::D24_UNORM_S8_UINT);
  REQUIRE(info.internal_format != 0);
}

TEST_CASE("GlFormatInfo: D32_FLOAT produces valid depth triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth formats
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::D32_FLOAT);
  REQUIRE(info.internal_format != 0);
}

TEST_CASE("GlFormatInfo: D32_FLOAT_S8_UINT produces valid depth triplet",
          "[opengl][types]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Depth/stencil formats
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::D32_FLOAT_S8_UINT);
  REQUIRE(info.internal_format != 0);
}

// ============================================================================
// GlFormatInfo::fromRhiFormat — edge cases
// ============================================================================

TEST_CASE("GlFormatInfo: UNDEFINED returns zero triplet", "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Undefined format
  auto info = GlFormatInfo::fromRhiFormat(RhiFormat::UNDEFINED);
  REQUIRE(info.internal_format == 0);
  REQUIRE(info.format == 0);
  REQUIRE(info.type == 0);
}

TEST_CASE("GlFormatInfo: BGRA formats produce valid triplets",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — BGR formats
  auto info_unorm = GlFormatInfo::fromRhiFormat(RhiFormat::BGR_A8_UNORM);
  auto info_srgb = GlFormatInfo::fromRhiFormat(RhiFormat::BGR_A8_SRGB);
  REQUIRE(info_unorm.internal_format != 0);
  REQUIRE(info_srgb.internal_format != 0);
}

TEST_CASE("GlFormatInfo: compressed formats produce valid triplets",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Compressed formats
  auto bc7_unorm = GlFormatInfo::fromRhiFormat(RhiFormat::B_C7_UNORM);
  auto bc7_srgb = GlFormatInfo::fromRhiFormat(RhiFormat::B_C7_SRGB);
  auto astc_unorm = GlFormatInfo::fromRhiFormat(RhiFormat::ASTC4X4_UNORM);
  auto astc_srgb = GlFormatInfo::fromRhiFormat(RhiFormat::ASTC4X4_SRGB);
  REQUIRE(bc7_unorm.internal_format != 0);
  REQUIRE(bc7_srgb.internal_format != 0);
  REQUIRE(astc_unorm.internal_format != 0);
  REQUIRE(astc_srgb.internal_format != 0);
}

// ============================================================================
// GlFormatInfo::componentCount — vertex attribute formats
// ============================================================================

TEST_CASE("GlFormatInfo: componentCount for RGBA32F is 4", "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3 —
  //   Vertex attribute conversion
  REQUIRE(GlFormatInfo::componentCount(RhiFormat::RGB_A32_FLOAT) == 4);
}

TEST_CASE("GlFormatInfo: componentCount for RG32F is 2", "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3
  REQUIRE(GlFormatInfo::componentCount(RhiFormat::R_G32_FLOAT) == 2);
}

TEST_CASE("GlFormatInfo: componentCount for R32F is 1", "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3
  REQUIRE(GlFormatInfo::componentCount(RhiFormat::R32_FLOAT) == 1);
}

TEST_CASE("GlFormatInfo: componentCount for RGBA8 is 4", "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3
  REQUIRE(GlFormatInfo::componentCount(RhiFormat::RGB_A8_UNORM) == 4);
}

TEST_CASE("GlFormatInfo: componentType returns GL type", "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3
  auto float_type = GlFormatInfo::componentType(RhiFormat::RGB_A32_FLOAT);
  auto byte_type = GlFormatInfo::componentType(RhiFormat::RGB_A8_UNORM);
  REQUIRE(float_type != 0);
  REQUIRE(byte_type != 0);
  REQUIRE(float_type != byte_type);
}

TEST_CASE("GlFormatInfo: bytesPerTexel matches unpack layout",
          "[opengl][types]") {
  REQUIRE(GlFormatInfo::bytesPerTexel(RhiFormat::UNDEFINED) == 0);
  REQUIRE(GlFormatInfo::bytesPerTexel(RhiFormat::R8_UNORM) == 1);
  REQUIRE(GlFormatInfo::bytesPerTexel(RhiFormat::R_G8_UNORM) == 2);
  REQUIRE(GlFormatInfo::bytesPerTexel(RhiFormat::RGB_A8_UNORM) == 4);
  REQUIRE(GlFormatInfo::bytesPerTexel(RhiFormat::B_C7_UNORM) == 0);
}

// ============================================================================
// toGlTopology — primitive topology conversion
// ============================================================================

TEST_CASE("toGlTopology: all topologies produce distinct values",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Primitive topology
  auto triangles = toGlTopology(RhiPrimitiveTopology::TRIANGLE_LIST);
  auto strips = toGlTopology(RhiPrimitiveTopology::TRIANGLE_STRIP);
  auto lines = toGlTopology(RhiPrimitiveTopology::LINE_LIST);
  auto points = toGlTopology(RhiPrimitiveTopology::POINT_LIST);

  // GL_POINTS is 0x0000, so we can't assert non-zero for all.
  // Instead, verify all four are distinct from each other.
  REQUIRE(triangles != strips);
  REQUIRE(triangles != lines);
  REQUIRE(triangles != points);
  REQUIRE(strips != lines);
  REQUIRE(strips != points);
  REQUIRE(lines != points);
}

// ============================================================================
// toGlStorageFlags — buffer usage flag conversion
// ============================================================================

TEST_CASE("toGlStorageFlags: host-visible buffer includes map flags",
          "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3 —
  //   Buffer usage mapping
  auto flags =
      toGlStorageFlags(RhiBufferUsage::VERTEX, GlHostVisibility::HOST_VISIBLE);
  REQUIRE(flags != 0);
}

TEST_CASE("toGlStorageFlags: non-host-visible buffer has no map flags",
          "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3
  auto flags_host =
      toGlStorageFlags(RhiBufferUsage::VERTEX, GlHostVisibility::HOST_VISIBLE);
  auto flags_device =
      toGlStorageFlags(RhiBufferUsage::VERTEX, GlHostVisibility::DEVICE_LOCAL);
  REQUIRE(flags_host != flags_device);
}

TEST_CASE("toGlStorageFlags: staging buffer includes dynamic storage",
          "[opengl][types]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3
  auto flags =
      toGlStorageFlags(RhiBufferUsage::STAGING, GlHostVisibility::HOST_VISIBLE);
  REQUIRE(flags != 0);
}

// ============================================================================
// toGlIndexType — index type conversion
// ============================================================================

TEST_CASE("toGlIndexType: Uint16 and Uint32 produce distinct values",
          "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Index types
  auto u16 = toGlIndexType(RhiIndexType::UINT16);
  auto u32 = toGlIndexType(RhiIndexType::UINT32);
  REQUIRE(u16 != 0);
  REQUIRE(u32 != 0);
  REQUIRE(u16 != u32);
}

// ============================================================================
// indexTypeSize — byte size per index element
// ============================================================================

TEST_CASE("indexTypeSize: Uint16 is 2 bytes", "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Index buffer
  REQUIRE(indexTypeSize(RhiIndexType::UINT16) == 2);
}

TEST_CASE("indexTypeSize: Uint32 is 4 bytes", "[opengl][types]") {
  // Req: docs/engine/rendering/rhi-shader-extension.md §1 — Index buffer
  REQUIRE(indexTypeSize(RhiIndexType::UINT32) == 4);
}

#endif  // ENGINE_RENDERER_OPENGL
