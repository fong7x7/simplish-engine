#include <catch2/catch_test_macros.hpp>

// The volumetric effects renderer on whichever GPU backend the build
// selected: a scene pass leaves depth behind, a cloud of smoke is marched
// against it, and the target is read back. Skips where the machine has no
// device, or the backend has no built-in volume pipeline.

#include "support/gpu_test_context.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <engine/render-fx/fx-volume-renderer.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-texture-desc.h>
#include <vector>

using namespace eng;
using eng::test::GPU_TEST_SIZE;
using eng::test::GpuTestContext;

namespace {

/// The depth the scene pass leaves everywhere: halfway back.
constexpr float SCENE_DEPTH = 0.5f;

/// The BGRA texel at (x, y), row 0 at the top.
std::array<int, 4> texelAt(const std::vector<uint8_t>& texels, uint32_t x,
                           uint32_t y) {
  const size_t i = (static_cast<size_t>(y) * GPU_TEST_SIZE + x) * 4;
  return {texels[i], texels[i + 1], texels[i + 2], texels[i + 3]};
}

/// Whether `device` is real and ships the volume pipeline.
bool hasVolumePipeline(RhiDevice* device) {
  RhiPipelineHandle probe = RHI_PIPELINE_INVALID;
  if (device == nullptr || !device->tryCreateFxVolumePipeline(probe)) {
    return false;
  }
  device->destroyPipeline(probe);
  return true;
}

/// A depth target the effects pass can sample, as `MeshRenderer` makes one.
RhiTextureHandle createDepth(RhiDevice& device) {
  RhiTextureDesc desc{};
  desc.width = GPU_TEST_SIZE;
  desc.height = GPU_TEST_SIZE;
  desc.format = RhiFormat::D32_FLOAT;
  desc.usage = RhiTextureUsage::DEPTH_STENCIL | RhiTextureUsage::SAMPLED;
  return device.createTexture(desc);
}

/// Add a cube of thick pale smoke half a quarter of clip space across,
/// centred at clip (@p x, 0, @p z), at the thickest point of its life.
void addVolume(FxVolumePool& pool, float x, float z) {
  const uint32_t i = pool.live++;
  pool.position[i] = {x, 0.0f, z};
  pool.age[i] = FX_VOLUME_FADE_IN;
  pool.seed[i] = 5.0f;
  pool.scale[i] = 1.0f;
  FxVolume look;
  look.color = {0.8f, 0.8f, 0.8f, 1.0f};
  look.density = 12.0f;
  look.radius = 0.25f;
  look.height = 0.25f;
  look.life = 1.0f;
  pool.look[i] = look;
}

/// The scene pass: colour cleared to black, and depth to `SCENE_DEPTH`.
void recordScenePass(RhiCommandList& cmd, RhiTextureHandle& target,
                     RhiTextureHandle depth) {
  RhiRenderPassBeginInfo scene{};
  scene.color_targets = &target;
  scene.color_target_count = 1;
  scene.depth_target = depth;
  scene.depth_load_op = RhiLoadOp::CLEAR;
  scene.clear_depth = SCENE_DEPTH;
  cmd.beginRenderPass(scene);
  cmd.endRenderPass();
}

/// The pass after it, with no depth attached, that the smoke is drawn in.
void beginOverPass(RhiCommandList& cmd, RhiTextureHandle& target) {
  RhiRenderPassBeginInfo over{};
  over.color_targets = &target;
  over.color_target_count = 1;
  over.color_load_op = RhiLoadOp::LOAD;
  cmd.beginRenderPass(over);
}

/// Draw @p volumes under a matrix that leaves world coordinates as clip
/// coordinates, and read the target back.
std::vector<uint8_t> renderVolumes(const GpuTestContext& ctx,
                                   FxVolumeRenderer& fx,
                                   const FxVolumePool& volumes) {
  RhiDevice& device = *ctx.device();
  RhiTextureHandle target = ctx.createColorTarget();
  const RhiTextureHandle depth = createDepth(device);
  const auto size = static_cast<float>(GPU_TEST_SIZE);
  const FxVolumeRenderer::DrawParams params{
      &volumes,
      depth,
      Mat4::identity(),
      {0.0f, 0.0f, size, size, 0.0f, 1.0f},
      {0, 0, GPU_TEST_SIZE, GPU_TEST_SIZE}};
  return ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
    recordScenePass(cmd, target, depth);
    beginOverPass(cmd, target);
    fx.draw(device, cmd, params);
    cmd.endRenderPass();
  });
}

/// The blue channel of nine texels spread over the square of side
/// @p reach about (@p cx, half height).
std::vector<int> patchAt(const std::vector<uint8_t>& texels, uint32_t cx,
                         uint32_t reach) {
  std::vector<int> patch;
  for (uint32_t k = 0; k < 9; ++k) {
    const uint32_t x = cx + (k % 3) * reach - reach;
    const uint32_t y = GPU_TEST_SIZE / 2 + (k / 3) * reach - reach;
    patch.push_back(texelAt(texels, x, y)[2]);
  }
  return patch;
}

}  // namespace

// Req: docs/engine/fx.md — a cloud of smoke is marched between the camera
// and whatever the scene's depth says is there, so one in front of the
// surfaces shows and one behind them does not.
TEST_CASE("FxVolumeRenderer on the GPU: the scene's depth stops the march",
          "[gpu][fx-volume]") {
  GpuTestContext ctx;
  if (!hasVolumePipeline(ctx.device())) {
    SKIP("no GPU device with a built-in volume pipeline");
  }
  FxVolumeRenderer fx;
  REQUIRE(fx.init(*ctx.device(), 8));
  FxVolumePool volumes(8);
  addVolume(volumes, -0.5f, 0.2f);
  addVolume(volumes, 0.5f, 0.8f);
  const auto texels = renderVolumes(ctx, fx, volumes);
  REQUIRE_FALSE(texels.empty());

  // The cloud in front of the scene covers its middle; the one behind it
  // is gone, and so is the frame between them.
  CHECK(texelAt(texels, GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[2] > 100);
  CHECK(texelAt(texels, 3 * GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[2] < 8);
  CHECK(texelAt(texels, GPU_TEST_SIZE / 2, 2)[2] < 8);
  fx.shutdown(*ctx.device());
}

// Req: docs/engine/fx.md — the smoke is a field of noise marched through,
// not a flat shape, so it is uneven across its own face.
TEST_CASE("FxVolumeRenderer on the GPU: a cloud is uneven across its face",
          "[gpu][fx-volume]") {
  GpuTestContext ctx;
  if (!hasVolumePipeline(ctx.device())) {
    SKIP("no GPU device with a built-in volume pipeline");
  }
  FxVolumeRenderer fx;
  REQUIRE(fx.init(*ctx.device(), 8));
  FxVolumePool volumes(8);
  addVolume(volumes, 0.0f, 0.2f);
  const auto texels = renderVolumes(ctx, fx, volumes);
  REQUIRE_FALSE(texels.empty());

  const std::vector<int> patch = patchAt(texels, GPU_TEST_SIZE / 2, 4);
  const auto [low, high] = std::ranges::minmax_element(patch);
  CHECK(*high > 60);
  CHECK(*high - *low > 16);
  fx.shutdown(*ctx.device());
}
