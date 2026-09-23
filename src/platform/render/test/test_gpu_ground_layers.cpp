#include <catch2/catch_test_macros.hpp>

// Painted ground on whichever GPU backend the build selected: two cells
// side by side, each a different terrain, drawn through the mesh pipeline
// with a two-swatch atlas and read back. Each cell must come out in its own
// swatch's colour — which is the atlas layout, the texture coordinates and
// the layer stacking all agreeing — and the later terrain must win the
// depth test where it covers the earlier one. Skips where the machine has
// no device, or the backend has no built-in mesh pipeline.

#include "support/gpu_test_context.h"

#include <array>
#include <cstdint>
#include <engine/render-ground/ground-mesh.h>
#include <engine/render-mesh/mesh-renderer.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-texture-desc.h>
#include <vector>

using namespace eng;
using eng::test::GPU_TEST_SIZE;
using eng::test::GpuTestContext;

namespace {

/// The BGRA texel at (x, y), row 0 at the top.
std::array<int, 4> texelAt(const std::vector<uint8_t>& texels, uint32_t x,
                           uint32_t y) {
  const size_t i = (static_cast<size_t>(y) * GPU_TEST_SIZE + x) * 4;
  return {texels[i], texels[i + 1], texels[i + 2], texels[i + 3]};
}

/// Whether `device` is real and ships the mesh pipeline.
bool hasMeshPipeline(RhiDevice* device) {
  RhiPipelineHandle probe = RHI_PIPELINE_INVALID;
  if (device == nullptr || !device->tryCreateMeshPipeline(probe)) {
    return false;
  }
  device->destroyPipeline(probe);
  return true;
}

/// An atlas of two swatches, as `makeGroundMesh` addresses one: layer 1
/// red across the top half, layer 2 green across the bottom. RGBA.
std::vector<uint8_t> twoSwatchTexels() {
  const size_t texels =
      static_cast<size_t>(GROUND_SWATCH_TEXELS) * GROUND_SWATCH_TEXELS * 2;
  std::vector<uint8_t> pixels(texels * 4, 255);
  for (size_t i = 0; i < texels; ++i) {
    const bool top = i < texels / 2;
    pixels[i * 4] = top ? 255 : 0;
    pixels[i * 4 + 1] = top ? 0 : 255;
    pixels[i * 4 + 2] = 0;
  }
  return pixels;
}

/// That atlas, uploaded as the editor uploads its own.
RhiTextureHandle createAtlas(RhiDevice& device,
                             const std::vector<uint8_t>& pixels) {
  RhiTextureDesc desc{};
  desc.width = GROUND_SWATCH_TEXELS;
  desc.height = GROUND_SWATCH_TEXELS * 2;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.debug_name = "ground_layers_atlas";
  desc.initial_pixels = pixels.data();
  return device.createTexture(desc);
}

/// The depth target the mesh pass writes.
RhiTextureHandle createDepth(RhiDevice& device) {
  RhiTextureDesc desc{};
  desc.width = GPU_TEST_SIZE;
  desc.height = GPU_TEST_SIZE;
  desc.format = RhiFormat::D32_FLOAT;
  desc.usage = RhiTextureUsage::DEPTH_STENCIL | RhiTextureUsage::SAMPLED;
  return device.createTexture(desc);
}

/// A model matrix carrying the two cells, x 0 to 2 and y 0 to 1, onto the
/// whole of clip space, with height towards the viewer — as the editor's
/// camera, looking down, sees a higher layer as nearer.
Mat4 twoCellModel() {
  Mat4 model{};
  model(0, 0) = 1.0f;
  model(0, 3) = -1.0f;
  model(1, 1) = 2.0f;
  model(1, 3) = -1.0f;
  model(2, 2) = -1.0f;
  model(2, 3) = 0.5f;
  model(3, 3) = 1.0f;
  return model;
}

/// The scene pass: colour cleared to black, depth cleared behind the ground.
void beginScenePass(RhiCommandList& cmd, RhiTextureHandle& target,
                    RhiTextureHandle depth) {
  RhiRenderPassBeginInfo scene{};
  scene.color_targets = &target;
  scene.color_target_count = 1;
  scene.depth_target = depth;
  scene.depth_load_op = RhiLoadOp::CLEAR;
  scene.clear_depth = 1.0f;
  cmd.beginRenderPass(scene);
}

/// What one instance is drawn with: the whole surface, no camera.
MeshRenderer::DrawParams drawParams(const MeshInstance& instance) {
  const auto size = static_cast<float>(GPU_TEST_SIZE);
  MeshRenderer::DrawParams params{};
  params.view_projection = Mat4::identity();
  params.instances = {&instance, 1};
  params.viewport = {0.0f, 0.0f, size, size, 0.0f, 1.0f};
  params.scissor = {0, 0, GPU_TEST_SIZE, GPU_TEST_SIZE};
  return params;
}

/// Draw @p instance into a scene pass and read the target back.
std::vector<uint8_t> render(const GpuTestContext& ctx, MeshRenderer& mesh,
                            const MeshInstance& instance) {
  RhiTextureHandle target = ctx.createColorTarget();
  const RhiTextureHandle depth = createDepth(*ctx.device());
  const MeshRenderer::DrawParams params = drawParams(instance);
  return ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
    beginScenePass(cmd, target, depth);
    mesh.draw(cmd, params);
    cmd.endRenderPass();
  });
}

/// Terrain 1 on the west cell, terrain 2 on the east one.
MeshData twoTerrainMesh() {
  GroundGrid grid;
  grid.set({0, 0}, 1);
  grid.set({1, 0}, 2);
  return makeGroundMesh(grid, 2);
}

/// Upload the two-terrain ground and its atlas, draw it filling the
/// target, and read the target back. Empty when the upload failed.
std::vector<uint8_t> renderTwoTerrains(const GpuTestContext& ctx,
                                       MeshRenderer& mesh) {
  const auto uploaded = mesh.upload(*ctx.device(), twoTerrainMesh());
  if (!uploaded) {
    return {};
  }
  const std::vector<uint8_t> atlas = twoSwatchTexels();
  return render(ctx, mesh,
                {*uploaded, twoCellModel(), createAtlas(*ctx.device(), atlas)});
}

}  // namespace

// Req: Editor §4.1 — auto-tiling, drawn through the same mesh pass as
// everything else in the level.
TEST_CASE("MeshRenderer on the GPU: each painted cell shows its own terrain",
          "[gpu][ground]") {
  GpuTestContext ctx;
  if (!hasMeshPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in mesh pipeline");
  }
  MeshRenderer mesh;
  REQUIRE(mesh.init(*ctx.device()));
  const auto texels = renderTwoTerrains(ctx, mesh);
  REQUIRE_FALSE(texels.empty());
  // BGRA, at the middle of each cell: red on the west, green on the east,
  // whatever the lighting has made of their brightness.
  const auto west = texelAt(texels, GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2);
  const auto east = texelAt(texels, 3 * GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2);
  CHECK(west[2] > west[1] + 20);
  CHECK(east[1] > east[2] + 20);
  mesh.shutdown(*ctx.device());
}
