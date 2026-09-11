#include <catch2/catch_test_macros.hpp>

// VulkanCommandList tests draw with the built-in pipelines into an offscreen
// target and read it back, so they check what reaches the pixels rather
// than which calls were made. They skip where there is no Vulkan device.
#ifdef ENGINE_RENDERER_VULKAN

#include "support/gpu_test_context.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <engine/render/rhi-buffer-desc.h>
#include <engine/render/rhi-draw-indexed-params.h>
#include <engine/render/rhi-draw-params.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-texture-desc.h>
#include <engine/render/rhi-types.h>
#include <vector>

using namespace eng;
using eng::test::GPU_TEST_SIZE;
using eng::test::GpuTestContext;

// Req: docs/platform/REQUIREMENTS.md PLT-RHI-1 — the RHI calls the Metal
// backend is driven with produce the same picture here. Each test records
// what the renderer that owns the pipeline records, and nothing more.

namespace {

/// `eng::MeshVertex`, restated: position, normal, uv.
struct TestMeshVertex {
  /// Object-space position.
  std::array<float, 3> position{};
  /// Object-space normal.
  std::array<float, 3> normal{};
  /// Texture coordinate.
  std::array<float, 2> uv{};
};
static_assert(sizeof(TestMeshVertex) == 32);

/// `eng::SkinnedMeshVertex`, restated: a mesh vertex, four joint
/// indices, four weights.
struct TestSkinnedVertex {
  /// The static part.
  TestMeshVertex base{};
  /// Joint indices into the palette.
  std::array<uint8_t, 4> joints{};
  /// Weight of each joint.
  std::array<float, 4> weights{};
};
static_assert(sizeof(TestSkinnedVertex) == 52);

/// `eng::GuiVertex`, restated.
struct TestGuiVertex {
  /// Layout-space position.
  std::array<float, 2> position{};
  /// Texture coordinate.
  std::array<float, 2> uv{};
  /// Packed RGBA8, red in the low byte.
  uint32_t color = 0;
  /// Corner radius of a rounded rect.
  float corner_radius = 0.0f;
  /// Border width.
  float border_width = 0.0f;
  /// 0x2 textured, 0x4 rounded.
  uint32_t flags = 0;
  /// Rect size, for the rounded-rect coverage.
  std::array<float, 2> rect_wh{};
};
static_assert(sizeof(TestGuiVertex) == 40);

/// World-to-clip and object-to-world, both identity.
constexpr std::array<float, 32> IDENTITY_UNIFORMS{
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

/// `MeshFragmentLights` with no lights: count, bands, two pads, and
/// eight lights of three float4s.
constexpr std::array<uint32_t, 4 + (8 * 12)> NO_LIGHTS{};

/// A `SkinPalette` whose joint 0 is the identity, three rows of it.
constexpr std::array<float, 12> IDENTITY_JOINT{1, 0, 0, 0, 0, 1,
                                               0, 0, 0, 0, 1, 0};

/// Two triangles over a rectangle.
constexpr std::array<uint32_t, 6> QUAD_INDICES{0, 1, 2, 0, 2, 3};

/// What the mesh shader writes for an unlit white surface: the ambient
/// term, 0.38, stored in an sRGB target — 0.38 of 255.
constexpr int AMBIENT_BYTE = 97;

/// Allowance for rounding on the way through the sRGB conversions.
constexpr int BYTE_TOLERANCE = 2;

/// Opaque black, the clear colour of every test pass, as BGRA.
constexpr std::array<int, 4> CLEAR_BGRA{0, 0, 0, 255};

/// Middle of the target, which every quad here covers or avoids whole.
constexpr uint32_t MID = GPU_TEST_SIZE / 2;

/// The BGRA texel at (x, y), row 0 at the top.
std::array<int, 4> texelAt(const std::vector<uint8_t>& texels, uint32_t x,
                           uint32_t y) {
  const size_t i = (static_cast<size_t>(y) * GPU_TEST_SIZE + x) * 4;
  return {texels[i], texels[i + 1], texels[i + 2], texels[i + 3]};
}

/// Whether `texel` is `expected`, within rounding.
bool near(const std::array<int, 4>& texel, const std::array<int, 4>& expected) {
  for (size_t i = 0; i < texel.size(); ++i) {
    if (std::abs(texel[i] - expected[i]) > BYTE_TOLERANCE) {
      return false;
    }
  }
  return true;
}

RhiBufferHandle upload(RhiDevice& device, const void* data, uint64_t size,
                       RhiBufferUsage usage) {
  RhiBufferDesc desc{};
  desc.size = size;
  desc.usage = usage;
  desc.host_visible = true;
  const RhiBufferHandle buffer = device.createBuffer(desc);
  std::memcpy(device.mapBuffer(buffer), data, size);
  device.unmapBuffer(buffer);
  return buffer;
}

RhiTextureHandle createDepthTarget(RhiDevice& device) {
  RhiTextureDesc desc{};
  desc.width = GPU_TEST_SIZE;
  desc.height = GPU_TEST_SIZE;
  desc.format = RhiFormat::D32_FLOAT;
  desc.usage = RhiTextureUsage::DEPTH_STENCIL | RhiTextureUsage::SAMPLED;
  return device.createTexture(desc);
}

/// A 1x1 texture of one RGBA8 colour.
RhiTextureHandle createSolidTexture(RhiDevice& device,
                                    const std::array<uint8_t, 4>& rgba) {
  RhiTextureDesc desc{};
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.initial_pixels = rgba.data();
  return device.createTexture(desc);
}

/// A pass that clears `target` to opaque black, and `depth` to far when
/// one is given.
RhiRenderPassBeginInfo clearPass(const RhiTextureHandle* target,
                                 RhiTextureHandle depth) {
  RhiRenderPassBeginInfo rp{};
  rp.color_targets = target;
  rp.color_target_count = 1;
  rp.depth_target = depth;
  rp.clear_depth = 1.0f;
  return rp;
}

/// A quad over the top half of clip space at depth `z`. With clip-space
/// +Y up, as Metal and D3D have it, that is the top rows of the target.
std::array<TestMeshVertex, 4> topHalfQuad(float z) {
  return {{{{-1, 0, z}, {0, 0, 1}, {0, 0}},
           {{1, 0, z}, {0, 0, 1}, {1, 0}},
           {{1, 1, z}, {0, 0, 1}, {1, 1}},
           {{-1, 1, z}, {0, 0, 1}, {0, 1}}}};
}

/// The same quad, every vertex bound wholly to joint 0.
std::array<TestSkinnedVertex, 4> skinnedTopHalfQuad(float z) {
  std::array<TestSkinnedVertex, 4> skinned{};
  const auto quad = topHalfQuad(z);
  for (size_t i = 0; i < quad.size(); ++i) {
    skinned[i] = {quad[i], {0, 0, 0, 0}, {1, 0, 0, 0}};
  }
  return skinned;
}

/// A GUI quad over the left half of the target, in layout pixels.
std::array<TestGuiVertex, 4> leftHalfGuiQuad(uint32_t color, uint32_t flags) {
  constexpr float W = GPU_TEST_SIZE / 2.0f;
  constexpr float H = GPU_TEST_SIZE;
  return {{{{0, 0}, {0, 0}, color, 0, 0, flags, {W, H}},
           {{W, 0}, {1, 0}, color, 0, 0, flags, {W, H}},
           {{W, H}, {1, 1}, color, 0, 0, flags, {W, H}},
           {{0, H}, {0, 1}, color, 0, 0, flags, {W, H}}}};
}

/// One indexed quad's inputs.
struct QuadDraw {
  /// Vertex buffer, in the pipeline's vertex layout.
  RhiBufferHandle vertices = RHI_BUFFER_INVALID;
  /// `QUAD_INDICES`.
  RhiBufferHandle indices = RHI_BUFFER_INVALID;
  /// The pipeline to draw with.
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  /// Texture to bind at slot 0, or invalid to bind none.
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
};

/// Upload a quad's vertices and indices, to draw with `pipeline`.
template <typename Vertex>
QuadDraw uploadQuad(RhiDevice& device, const std::array<Vertex, 4>& quad,
                    RhiPipelineHandle pipeline) {
  QuadDraw draw;
  draw.vertices =
      upload(device, quad.data(), sizeof(quad), RhiBufferUsage::VERTEX);
  draw.indices = upload(device, QUAD_INDICES.data(), sizeof(QUAD_INDICES),
                        RhiBufferUsage::INDEX);
  draw.pipeline = pipeline;
  return draw;
}

/// Bind the quad's texture and buffers, and draw it.
void drawQuad(RhiCommandList& cmd, const QuadDraw& draw) {
  if (draw.texture != RHI_TEXTURE_INVALID) {
    cmd.bindFragmentTexture(draw.texture, 0);
  }
  cmd.bindVertexBuffer(draw.vertices);
  cmd.bindIndexBuffer(draw.indices, 0, RhiIndexType::UINT32);
  RhiDrawIndexedParams params{};
  params.index_count = static_cast<uint32_t>(QUAD_INDICES.size());
  cmd.drawIndexed(params);
}

/// What `MeshRenderer` and `SkinnedMeshRenderer` set before a draw: the
/// uniforms at vertex slot 1, the joint palette at 2, the lights at
/// fragment slot 0. Static meshes ignore the palette.
void recordMeshDraw(RhiCommandList& cmd, const QuadDraw& draw) {
  cmd.bindPipeline(draw.pipeline);
  cmd.setVertexStageBytes(IDENTITY_UNIFORMS.data(), sizeof(IDENTITY_UNIFORMS),
                          1);
  cmd.setVertexStageBytes(IDENTITY_JOINT.data(), sizeof(IDENTITY_JOINT), 2);
  cmd.setFragmentStageBytes(NO_LIGHTS.data(), sizeof(NO_LIGHTS), 0);
  drawQuad(cmd, draw);
}

/// What `GuiRenderer` sets before a batch: layout pixels to clip space
/// at vertex slot 1.
void recordGuiDraw(RhiCommandList& cmd, const QuadDraw& draw) {
  cmd.bindPipeline(draw.pipeline);
  const std::array<float, 2> scale{2.0f / GPU_TEST_SIZE, 2.0f / GPU_TEST_SIZE};
  cmd.setVertexStageBytes(scale.data(), sizeof(scale), 1);
  drawQuad(cmd, draw);
}

/// What `MeshOutlineRenderer` records: its uniforms — blue, bounds over
/// the whole target, one pixel wide, a small threshold — the scene's
/// depth at slot 0, and one full-screen triangle.
void recordOutline(RhiCommandList& cmd, RhiPipelineHandle pipeline,
                   RhiTextureHandle depth) {
  constexpr auto S = static_cast<float>(GPU_TEST_SIZE);
  const std::array<float, 12> uniforms{0, 0, 1, 1, 0, 0, S, S, 1, 0.01f, 0, 0};
  cmd.bindPipeline(pipeline);
  cmd.setFragmentStageBytes(uniforms.data(), sizeof(uniforms), 0);
  cmd.bindFragmentTexture(depth, 0);
  RhiDrawParams params{};
  params.vertex_count = 3;
  cmd.draw(params);
}

/// Records one draw into a pass that is already open.
using DrawRecorder = void (*)(RhiCommandList&, const QuadDraw&);

/// Whether the one pass a test draws has a depth attachment.
enum class DepthAttachment { NONE, D32 };

/// Clear, record `record` in one pass, and read the target back.
std::vector<uint8_t> renderPass(const GpuTestContext& ctx, const QuadDraw& draw,
                                DrawRecorder record, DepthAttachment depth) {
  const RhiTextureHandle target = ctx.createColorTarget();
  const RhiTextureHandle depth_target = depth == DepthAttachment::D32
                                            ? createDepthTarget(*ctx.device())
                                            : RHI_TEXTURE_INVALID;
  return ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
    cmd.beginRenderPass(clearPass(&target, depth_target));
    record(cmd, draw);
    cmd.endRenderPass();
  });
}

/// The frame the editor draws with an outlined selection: the scene
/// pass with depth, then a pass without it that reads that depth.
std::vector<uint8_t> renderOutlined(const GpuTestContext& ctx,
                                    const QuadDraw& scene,
                                    RhiPipelineHandle outline) {
  const RhiTextureHandle target = ctx.createColorTarget();
  const RhiTextureHandle depth = createDepthTarget(*ctx.device());
  return ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
    cmd.beginRenderPass(clearPass(&target, depth));
    recordMeshDraw(cmd, scene);
    cmd.endRenderPass();
    RhiRenderPassBeginInfo overlay = clearPass(&target, RHI_TEXTURE_INVALID);
    overlay.color_load_op = RhiLoadOp::LOAD;
    cmd.beginRenderPass(overlay);
    recordOutline(cmd, outline, depth);
    cmd.endRenderPass();
  });
}

/// A static mesh draw of the top-half quad at depth 0.5.
QuadDraw staticQuad(RhiDevice& device) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  REQUIRE(device.tryCreateMeshPipeline(pipeline));
  return uploadQuad(device, topHalfQuad(0.5f), pipeline);
}

/// A GUI draw of the left-half quad.
QuadDraw guiQuad(RhiDevice& device, uint32_t color, uint32_t flags) {
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  REQUIRE(device.tryCreateGuiPipeline(pipeline));
  return uploadQuad(device, leftHalfGuiQuad(color, flags), pipeline);
}

}  // namespace

TEST_CASE("VulkanCommandList: a cleared pass reads back its clear colour",
          "[vulkan][command_list][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  const RhiTextureHandle target = ctx.createColorTarget();
  const auto texels = ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
    RhiRenderPassBeginInfo rp = clearPass(&target, RHI_TEXTURE_INVALID);
    rp.clear_color[0] = 1.0f;
    cmd.beginRenderPass(rp);
    cmd.endRenderPass();
  });
  REQUIRE(texels.size() == GPU_TEST_SIZE * GPU_TEST_SIZE * 4);
  CHECK(texelAt(texels, 5, 5) == std::array<int, 4>{0, 0, 255, 255});
}

TEST_CASE("VulkanCommandList: the GUI pipeline fills a quad with its colour",
          "[vulkan][command_list][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  // Opaque green, packed as the GUI packs it: red in the low byte.
  const QuadDraw draw = guiQuad(*ctx.device(), 0xFF00FF00u, 0);
  const auto texels =
      renderPass(ctx, draw, recordGuiDraw, DepthAttachment::NONE);
  REQUIRE_FALSE(texels.empty());
  CHECK(texelAt(texels, MID / 2, MID) == std::array<int, 4>{0, 255, 0, 255});
  CHECK(texelAt(texels, MID + (MID / 2), MID) == CLEAR_BGRA);
}

TEST_CASE("VulkanCommandList: a textured GUI quad samples its texture",
          "[vulkan][command_list][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  QuadDraw draw = guiQuad(*ctx.device(), 0xFFFFFFFFu, 2);
  draw.texture = createSolidTexture(*ctx.device(), {0, 0, 255, 255});
  const auto texels =
      renderPass(ctx, draw, recordGuiDraw, DepthAttachment::NONE);
  REQUIRE_FALSE(texels.empty());
  // Blue in, times a white vertex colour: blue out, in BGRA's first byte.
  CHECK(texelAt(texels, MID / 2, MID) == std::array<int, 4>{255, 0, 0, 255});
}

TEST_CASE("VulkanCommandList: clip-space +Y is up, as it is on Metal",
          "[vulkan][command_list][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  const auto texels = renderPass(ctx, staticQuad(*ctx.device()), recordMeshDraw,
                                 DepthAttachment::D32);
  REQUIRE_FALSE(texels.empty());
  // The unlit quad is ambient grey on top; the bottom is still the clear.
  const std::array<int, 4> grey{AMBIENT_BYTE, AMBIENT_BYTE, AMBIENT_BYTE, 255};
  CHECK(near(texelAt(texels, MID, 4), grey));
  CHECK(texelAt(texels, MID, GPU_TEST_SIZE - 4) == CLEAR_BGRA);
}

TEST_CASE("VulkanCommandList: the mesh pipeline shades its bound texture",
          "[vulkan][command_list][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  QuadDraw draw = staticQuad(*ctx.device());
  draw.texture = createSolidTexture(*ctx.device(), {255, 0, 0, 255});
  const auto texels =
      renderPass(ctx, draw, recordMeshDraw, DepthAttachment::D32);
  REQUIRE_FALSE(texels.empty());
  const std::array<int, 4> ambient_red{0, 0, AMBIENT_BYTE, 255};
  CHECK(near(texelAt(texels, MID, 4), ambient_red));
}

TEST_CASE("VulkanCommandList: a skinned quad on an identity joint draws "
          "where the static one does",
          "[vulkan][command_list][gpu]") {
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiPipelineHandle pipeline = RHI_PIPELINE_INVALID;
  REQUIRE(ctx.device()->tryCreateSkinnedMeshPipeline(pipeline));
  const QuadDraw draw =
      uploadQuad(*ctx.device(), skinnedTopHalfQuad(0.5f), pipeline);
  const auto texels =
      renderPass(ctx, draw, recordMeshDraw, DepthAttachment::D32);
  REQUIRE_FALSE(texels.empty());
  const std::array<int, 4> grey{AMBIENT_BYTE, AMBIENT_BYTE, AMBIENT_BYTE, 255};
  CHECK(near(texelAt(texels, MID, 4), grey));
  CHECK(texelAt(texels, MID, GPU_TEST_SIZE - 4) == CLEAR_BGRA);
}

TEST_CASE("VulkanCommandList: the outline lines the edge of the scene depth",
          "[vulkan][command_list][gpu]") {
  // The scene pass leaves its depth attached; the outline pass samples it.
  // Only a backend that moves the depth to a sampled layout between the two
  // gets a line here, and only one that reads the right rows gets it on
  // the quad's lower edge, the last row it covers.
  GpuTestContext ctx;
  if (ctx.device() == nullptr) {
    SKIP("no Vulkan device on this machine");
  }
  RhiPipelineHandle outline = RHI_PIPELINE_INVALID;
  REQUIRE(ctx.device()->tryCreateMeshOutlinePipeline(outline));
  const auto texels = renderOutlined(ctx, staticQuad(*ctx.device()), outline);
  REQUIRE_FALSE(texels.empty());
  const std::array<int, 4> grey{AMBIENT_BYTE, AMBIENT_BYTE, AMBIENT_BYTE, 255};
  CHECK(texelAt(texels, MID, MID - 1) == std::array<int, 4>{255, 0, 0, 255});
  CHECK(near(texelAt(texels, MID, MID - 4), grey));
  CHECK(texelAt(texels, MID, MID + 4) == CLEAR_BGRA);
}

#endif  // ENGINE_RENDERER_VULKAN
