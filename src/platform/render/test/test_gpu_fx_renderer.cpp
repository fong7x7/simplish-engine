#include <catch2/catch_test_macros.hpp>

// The effects renderer on whichever GPU backend the build selected: a scene
// pass leaves depth behind, the effects pass reads it, and the target is read
// back. Skips where the machine has no device, or the backend has no built-in
// effects pipeline.

#include "support/gpu_test_context.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <engine/render-fx/fx-renderer.h>
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

/// Whether `device` is real and ships the effects pipeline.
bool hasFxPipeline(RhiDevice* device) {
  RhiPipelineHandle probe = RHI_PIPELINE_INVALID;
  if (device == nullptr || !device->tryCreateFxParticlePipeline(probe)) {
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

/// Add a round particle a quarter of clip space across — sixteen pixels —
/// centred at clip (@p x, 0) and at depth @p z, coloured @p color.
void addParticle(FxParticlePool& pool, float x, float z, FxColor color) {
  const uint32_t i = pool.live++;
  pool.position[i] = {x, 0.0f, z};
  pool.life[i] = 1.0f;
  pool.scale[i] = 1.0f;
  FxParticleLook look;
  look.size_start = 0.25f;
  look.size_end = 0.25f;
  look.color_start = color;
  look.color_end = color;
  pool.look[i] = look;
}

/// A white puff on the left of the frame and a white disc on the right,
/// both in front of the scene.
void addPuffAndDisc(FxParticlePool& pool) {
  addParticle(pool, -0.5f, 0.1f, {1.0f, 1.0f, 1.0f, 0.0f});
  pool.look[0].shape = FxParticleShape::PUFF;
  pool.angle[0] = 3.0f;
  addParticle(pool, 0.5f, 0.1f, {1.0f, 1.0f, 1.0f, 0.0f});
}

/// The four texels that mirror one another about the centre of a quad
/// standing on the pixel boundary @p cx, @p dx and @p dy out from it.
///
/// A disc's mask depends on nothing but how far out a fragment is, so all
/// four read the same whatever the radius works out to in pixels; the
/// noise a puff is broken up by has no such symmetry.
std::vector<int> mirroredAt(const std::vector<uint8_t>& texels, uint32_t cx,
                            uint32_t dx, uint32_t dy) {
  constexpr uint32_t CY = GPU_TEST_SIZE / 2;
  return {texelAt(texels, cx + dx, CY + dy)[2],
          texelAt(texels, cx - 1 - dx, CY + dy)[2],
          texelAt(texels, cx + dx, CY - 1 - dy)[2],
          texelAt(texels, cx - 1 - dx, CY - 1 - dy)[2]};
}

/// How far apart the highest and lowest of @p samples are.
int spreadOf(const std::vector<int>& samples) {
  const auto [low, high] = std::ranges::minmax_element(samples);
  return *high - *low;
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

/// The pass after it, with no depth attached, that the effects draw in.
void beginOverPass(RhiCommandList& cmd, RhiTextureHandle& target) {
  RhiRenderPassBeginInfo over{};
  over.color_targets = &target;
  over.color_target_count = 1;
  over.color_load_op = RhiLoadOp::LOAD;
  cmd.beginRenderPass(over);
}

/// What `RenderedGameClient` records around the effects, under a matrix
/// that leaves world coordinates as clip coordinates.
std::vector<uint8_t> renderFx(const GpuTestContext& ctx, FxRenderer& fx,
                              const FxParticlePool& particles) {
  RhiDevice& device = *ctx.device();
  RhiTextureHandle target = ctx.createColorTarget();
  const RhiTextureHandle depth = createDepth(device);
  const auto size = static_cast<float>(GPU_TEST_SIZE);
  const FxRenderer::DrawParams params{&particles,
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

}  // namespace

// Req: docs/engine/fx.md — a particle in front of the scene's surfaces adds
// its light; one behind them is hidden by them, though nothing wrote depth.
TEST_CASE("FxRenderer on the GPU: the scene's depth hides what is behind it",
          "[gpu][fx]") {
  GpuTestContext ctx;
  if (!hasFxPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in effects pipeline");
  }
  FxRenderer fx;
  REQUIRE(fx.init(*ctx.device(), 8));
  FxParticlePool particles(8);
  addParticle(particles, -0.5f, 0.1f, {1.0f, 0.0f, 0.0f, 0.0f});
  addParticle(particles, 0.5f, 0.9f, {0.0f, 1.0f, 0.0f, 0.0f});
  const auto texels = renderFx(ctx, fx, particles);
  REQUIRE_FALSE(texels.empty());

  // BGRA: the red glow in front shows at the left quad's centre, and the
  // green one behind the scene is gone.
  CHECK(texelAt(texels, GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[2] > 200);
  CHECK(texelAt(texels, 3 * GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[1] < 8);
  // Outside the quads the frame is as the scene left it.
  CHECK(texelAt(texels, 2, 2)[2] < 8);
  fx.shutdown(*ctx.device());
}

// Req: docs/engine/fx.md — premultiplied blending: alpha zero only adds light,
// and smoke of alpha one in front of a glow hides it.
TEST_CASE("FxRenderer on the GPU: smoke in front of a glow hides it",
          "[gpu][fx]") {
  GpuTestContext ctx;
  if (!hasFxPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in effects pipeline");
  }
  FxRenderer fx;
  REQUIRE(fx.init(*ctx.device(), 8));
  FxParticlePool particles(8);
  addParticle(particles, -0.5f, 0.2f, {1.0f, 0.0f, 0.0f, 0.0f});
  addParticle(particles, -0.5f, 0.05f, {0.0f, 0.0f, 0.0f, 1.0f});
  addParticle(particles, 0.5f, 0.2f, {1.0f, 0.0f, 0.0f, 0.0f});
  const auto texels = renderFx(ctx, fx, particles);
  REQUIRE_FALSE(texels.empty());

  // The glow on its own shows; behind the smoke's centre, about 98% of it
  // is hidden. The target is sRGB, which lifts the little linear light left
  // to around 30 of 255, so the check is against a fifth of the glow.
  const int glow = texelAt(texels, 3 * GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[2];
  CHECK(glow > 200);
  CHECK(texelAt(texels, GPU_TEST_SIZE / 4, GPU_TEST_SIZE / 2)[2] < glow / 5);
  fx.shutdown(*ctx.device());
}

// Req: docs/engine/fx.md — a textured particle is a puff of noise, uneven
// where a disc is smooth, and no two of them are broken up alike.
TEST_CASE("FxRenderer on the GPU: a puff is uneven where a disc is smooth",
          "[gpu][fx]") {
  GpuTestContext ctx;
  if (!hasFxPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in effects pipeline");
  }
  FxRenderer fx;
  REQUIRE(fx.init(*ctx.device(), 8));
  FxParticlePool particles(8);
  addPuffAndDisc(particles);
  const auto texels = renderFx(ctx, fx, particles);
  REQUIRE_FALSE(texels.empty());

  // A few pixels out from each centre: the disc reads the same in all four
  // quadrants, and the puff does not.
  const std::vector<int> disc = mirroredAt(texels, 3 * GPU_TEST_SIZE / 4, 2, 1);
  const std::vector<int> puff = mirroredAt(texels, GPU_TEST_SIZE / 4, 2, 1);
  CHECK(spreadOf(disc) <= 1);
  CHECK(spreadOf(puff) > 16);
  fx.shutdown(*ctx.device());
}
