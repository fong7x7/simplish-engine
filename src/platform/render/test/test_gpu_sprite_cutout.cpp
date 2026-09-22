#include <catch2/catch_test_macros.hpp>

// The mesh pipeline's alpha-test cutout on whichever GPU backend the build
// selected: a sprite billboard's quad is drawn with a map that is opaque
// down one half and empty down the other, and the target is read back. The
// empty half must not have drawn at all. Skips where the machine has no
// device, or the backend has no built-in mesh pipeline.

#include "support/gpu_test_context.h"

#include <array>
#include <cstdint>
#include <engine/render-mesh/mesh-renderer.h>
#include <engine/render-sprite/sprite-quad.h>
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

/// Two texels side by side: an opaque red one, and an empty one. RGBA, as
/// the editor uploads a sheet.
constexpr uint8_t CUTOUT_TEXELS[] = {255, 0, 0, 255, 255, 0, 0, 0};

/// That pair, uploaded as a sheet would be.
RhiTextureHandle createCutoutSheet(RhiDevice& device) {
  RhiTextureDesc desc{};
  desc.width = 2;
  desc.height = 1;
  desc.format = RhiFormat::RGB_A8_UNORM;
  desc.usage = RhiTextureUsage::SAMPLED;
  desc.debug_name = "sprite_cutout_sheet";
  desc.initial_pixels = CUTOUT_TEXELS;
  return device.createTexture(desc);
}

/// The depth target the mesh pass writes, cleared behind the quad.
RhiTextureHandle createDepth(RhiDevice& device) {
  RhiTextureDesc desc{};
  desc.width = GPU_TEST_SIZE;
  desc.height = GPU_TEST_SIZE;
  desc.format = RhiFormat::D32_FLOAT;
  desc.usage = RhiTextureUsage::DEPTH_STENCIL | RhiTextureUsage::SAMPLED;
  return device.createTexture(desc);
}

/// A model matrix carrying the quad's local frame onto the whole of clip
/// space: local X across it, local Z up it, and the local +Y the normal
/// sits on pointed at the viewer so the quad is lit rather than in shadow.
Mat4 fullScreenQuadModel() {
  Mat4 model{};
  model(0, 0) = 2.0f;
  model(2, 1) = 1.0f;
  model(1, 2) = 2.0f;
  model(1, 3) = -1.0f;
  model(2, 3) = 0.5f;
  model(3, 3) = 1.0f;
  return model;
}

/// The scene pass: colour cleared to black, depth cleared behind the quad.
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
std::vector<uint8_t> renderQuad(const GpuTestContext& ctx, MeshRenderer& mesh,
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

}  // namespace

// Req: ADR-003 — alpha-test cutout gives sprite edges that depth-write
// correctly, which is what lets a billboard go through the opaque pass.
TEST_CASE("MeshRenderer on the GPU: texels under the cutoff do not draw",
          "[gpu][sprite]") {
  GpuTestContext ctx;
  if (!hasMeshPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in mesh pipeline");
  }
  MeshRenderer mesh;
  REQUIRE(mesh.init(*ctx.device()));
  const auto uploaded = mesh.upload(*ctx.device(), makeSpriteQuadMesh({}));
  REQUIRE(uploaded.has_value());
  const MeshInstance instance{*uploaded, fullScreenQuadModel(),
                              createCutoutSheet(*ctx.device())};
  const auto texels = renderQuad(ctx, mesh, instance);
  REQUIRE_FALSE(texels.empty());

  // BGRA, read at the centre of each half — the exact centre of a texel,
  // so bilinear filtering samples that texel alone. The opaque half is red;
  // the empty half is the black the pass cleared to.
  CHECK(texelAt(texels, GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[2] > 150);
  CHECK(texelAt(texels, 3 * GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[2] < 8);
  mesh.shutdown(*ctx.device());
}
