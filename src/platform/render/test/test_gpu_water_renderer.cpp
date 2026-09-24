#include <catch2/catch_test_macros.hpp>

// The water surface on whichever GPU backend the build selected: a pond
// filling the target, drawn in a scene pass over a cleared colour — the
// ground under the water, as far as the test is concerned — and against a
// cleared depth, and read back. Skips where the machine has no device, or
// the backend has no built-in water pipeline.

#include "support/gpu_test_context.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <engine/render-water/water-depth.h>
#include <engine/render-water/water-renderer.h>
#include <engine/render-water/water-surface-mesh.h>
#include <engine/render/rhi-render-pass-begin-info.h>
#include <engine/render/rhi-texture-desc.h>
#include <span>
#include <vector>

using namespace eng;
using eng::test::GPU_TEST_SIZE;
using eng::test::GpuTestContext;

namespace {

/// Half the pond's side, in tiles: it runs from −4 to 4 on both axes.
constexpr int32_t POND_HALF = 4;

/// The grey the "ground" under the pond is cleared to, linear.
constexpr float GROUND_GREY = 0.2f;

/// The BGRA texel at (x, y), row 0 at the top.
std::array<int, 4> texelAt(const std::vector<uint8_t>& texels, uint32_t x,
                           uint32_t y) {
  const size_t i = (static_cast<size_t>(y) * GPU_TEST_SIZE + x) * 4;
  return {texels[i], texels[i + 1], texels[i + 2], texels[i + 3]};
}

/// The texel in the middle of the target.
std::array<int, 4> middleOf(const std::vector<uint8_t>& texels) {
  return texelAt(texels, GPU_TEST_SIZE / 2, GPU_TEST_SIZE / 2);
}

/// Whether `device` is real and ships the water pipeline.
bool hasWaterPipeline(RhiDevice* device) {
  RhiPipelineHandle probe = RHI_PIPELINE_INVALID;
  if (device == nullptr || !device->tryCreateWaterPipeline(probe)) {
    return false;
  }
  device->destroyPipeline(probe);
  return true;
}

/// The depth target the scene pass holds.
RhiTextureHandle createDepth(RhiDevice& device) {
  RhiTextureDesc desc{};
  desc.width = GPU_TEST_SIZE;
  desc.height = GPU_TEST_SIZE;
  desc.format = RhiFormat::D32_FLOAT;
  desc.usage = RhiTextureUsage::DEPTH_STENCIL | RhiTextureUsage::SAMPLED;
  return device.createTexture(desc);
}

/// The pond: every cell from −4 to 3 on both axes the same @p water.
WaterLayer pond(const WaterCell& water) {
  WaterLayer layer;
  for (int32_t y = -POND_HALF; y < POND_HALF; ++y) {
    for (int32_t x = -POND_HALF; x < POND_HALF; ++x) {
      setWaterCell(layer, {x, y}, water);
    }
  }
  return layer;
}

/// Water @p depth tiles deep and @p opacity of 255 opaque, in the default
/// colour.
WaterCell waterOf(float depth, uint8_t opacity) {
  WaterCell water{};
  water.depth = waterDepthUnits(depth);
  water.opacity = opacity;
  return water;
}

/// Looking straight down on the pond, filling clip space with it: height
/// towards the eye, as the editor's camera sees a higher layer as nearer.
Mat4 lookingDown() {
  Mat4 m{};
  m(0, 0) = 1.0f / static_cast<float>(POND_HALF);
  m(1, 1) = 1.0f / static_cast<float>(POND_HALF);
  m(2, 2) = -1.0f;
  m(2, 3) = 0.5f;
  m(3, 3) = 1.0f;
  return m;
}

/// A light low from the east. The camera here looks straight down, and the
/// key light's glint off flat water faces it nearly head on, washing out
/// every colour; a light from the side lights the water without it.
constexpr MeshLight SIDE_LIGHTS[] = {MeshLight{{},
                                               0.0f,
                                               {0.9f, 0.0f, 0.44f},
                                               1.0f,
                                               {1.0f, 1.0f, 1.0f},
                                               MESH_LIGHT_DIRECTIONAL}};

/// How one draw of the pond is made.
struct PondDraw {
  /// What water the pond is.
  WaterCell water = waterOf(WATER_DEFAULT_DEPTH, WATER_DEFAULT_OPACITY);
  /// How much detail to shade with.
  WaterFidelity fidelity = WaterFidelity::LOW;
  /// What the scene pass clears its depth to: 1 is far behind the pond.
  float scene_depth = 1.0f;
  /// The lights it is lit by.
  std::span<const MeshLight> lights = SIDE_LIGHTS;
};

/// A draw of the pond as @p draw says, over the whole target.
WaterRenderer::DrawParams drawParams(const PondDraw& draw) {
  const auto size = static_cast<float>(GPU_TEST_SIZE);
  WaterRenderer::DrawParams params{};
  params.view_projection = lookingDown();
  params.viewport = {0.0f, 0.0f, size, size, 0.0f, 1.0f};
  params.scissor = {0, 0, GPU_TEST_SIZE, GPU_TEST_SIZE};
  params.fidelity = draw.fidelity;
  params.lights = draw.lights;
  params.seconds = 1.25f;
  return params;
}

/// Open the scene pass: colour cleared to the ground's grey, depth to
/// @p scene_depth.
void beginScenePass(RhiCommandList& cmd, RhiTextureHandle& target,
                    RhiTextureHandle depth, float scene_depth) {
  RhiRenderPassBeginInfo scene{};
  scene.color_targets = &target;
  scene.color_target_count = 1;
  scene.depth_target = depth;
  scene.depth_load_op = RhiLoadOp::CLEAR;
  scene.clear_depth = scene_depth;
  scene.clear_color[0] = GROUND_GREY;
  scene.clear_color[1] = GROUND_GREY;
  scene.clear_color[2] = GROUND_GREY;
  cmd.beginRenderPass(scene);
}

/// Draw the pond over @p field into a scene pass, as @p draw says, and read
/// the target back.
std::vector<uint8_t> renderPond(const GpuTestContext& ctx,
                                const WaterField& field, const PondDraw& draw) {
  RhiDevice& device = *ctx.device();
  WaterRenderer water;
  REQUIRE(water.init(device));
  REQUIRE(water.setSurface(device, makeWaterSurfaceMesh(pond(draw.water))));
  REQUIRE(water.setField(device, field));
  RhiTextureHandle target = ctx.createColorTarget();
  const RhiTextureHandle depth = createDepth(device);
  std::vector<uint8_t> texels =
      ctx.renderAndRead(target, [&](RhiCommandList& cmd) {
        beginScenePass(cmd, target, depth, draw.scene_depth);
        water.draw(cmd, drawParams(draw));
        cmd.endRenderPass();
      });
  water.shutdown(device);
  return texels;
}

/// A still field over a pond of @p water at @p samples_per_tile.
WaterField stillField(const WaterCell& water, uint32_t samples_per_tile = 8) {
  WaterField field;
  resetWaterField(field, pond(water), samples_per_tile);
  return field;
}

/// Draw a still pond as @p draw says.
std::vector<uint8_t> renderStill(const GpuTestContext& ctx,
                                 const PondDraw& draw) {
  return renderPond(ctx, stillField(draw.water), draw);
}

/// A bright red point light over the middle of the pond.
MeshLight redLamp() {
  MeshLight lamp{};
  lamp.kind = MESH_LIGHT_POINT;
  lamp.position = {0.0f, 0.0f, 1.5f};
  lamp.range = 6.0f;
  lamp.intensity = 2.0f;
  lamp.color = {1.0f, 0.0f, 0.0f};
  return lamp;
}

/// How far a texel is from the ground's grey: the sum of its channels'
/// distances from that grey's.
int offGrey(const std::array<int, 4>& texel, const std::array<int, 4>& grey) {
  return std::abs(texel[0] - grey[0]) + std::abs(texel[1] - grey[1]) +
         std::abs(texel[2] - grey[2]);
}

/// How far apart two read-backs are over the middle half of the target:
/// the sum of every channel's difference.
long differenceOverMiddle(const std::vector<uint8_t>& a,
                          const std::vector<uint8_t>& b) {
  long total = 0;
  for (uint32_t y = GPU_TEST_SIZE / 4; y < 3 * GPU_TEST_SIZE / 4; ++y) {
    for (uint32_t x = GPU_TEST_SIZE / 4; x < 3 * GPU_TEST_SIZE / 4; ++x) {
      const auto p = texelAt(a, x, y);
      const auto q = texelAt(b, x, y);
      total +=
          std::abs(p[0] - q[0]) + std::abs(p[1] - q[1]) + std::abs(p[2] - q[2]);
    }
  }
  return total;
}

/// The ground's grey as the target stores it: a pond hidden entirely
/// behind the scene's depth.
std::array<int, 4> groundGrey(const GpuTestContext& ctx) {
  return middleOf(renderStill(ctx, {.scene_depth = 0.0f}));
}

}  // namespace

// Req: docs/engine/water.md — the surface is drawn in the scene pass,
// tested against its depth: open water shows its colour, and anything
// nearer than the surface hides it.
TEST_CASE("WaterRenderer on the GPU: open water shows, and depth hides it",
          "[gpu][water]") {
  GpuTestContext ctx;
  if (!hasWaterPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in water pipeline");
  }
  const auto open = middleOf(renderStill(ctx, {}));
  const auto grey = groundGrey(ctx);
  CHECK(grey[0] == grey[2]);
  CHECK(open[0] > open[2] + 10);
  CHECK(offGrey(open, grey) > 20);
}

// Req: docs/engine/water.md §4 — water is translucent: clear water shows
// the ground under it, opaque water hides it under its own colour.
TEST_CASE("WaterRenderer on the GPU: clear water shows the ground",
          "[gpu][water]") {
  GpuTestContext ctx;
  if (!hasWaterPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in water pipeline");
  }
  const auto grey = groundGrey(ctx);
  const auto clear = middleOf(renderStill(ctx, {.water = waterOf(1.0f, 0)}));
  const auto murky = middleOf(renderStill(ctx, {.water = waterOf(1.0f, 255)}));
  CHECK(offGrey(clear, grey) < offGrey(murky, grey));
  CHECK(murky[0] > murky[2] + 20);
}

// Req: docs/engine/water.md §2 — deeper water hides more of the ground.
TEST_CASE("WaterRenderer on the GPU: deeper water hides more of the ground",
          "[gpu][water]") {
  GpuTestContext ctx;
  if (!hasWaterPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in water pipeline");
  }
  const auto grey = groundGrey(ctx);
  const auto shallow =
      middleOf(renderStill(ctx, {.water = waterOf(0.125f, 128)}));
  const auto deep = middleOf(renderStill(ctx, {.water = waterOf(4.0f, 128)}));
  CHECK(offGrey(shallow, grey) + 20 < offGrey(deep, grey));
}

// Req: docs/engine/water.md §6 — each body of water has its own colour.
TEST_CASE("WaterRenderer on the GPU: red water is red", "[gpu][water]") {
  GpuTestContext ctx;
  if (!hasWaterPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in water pipeline");
  }
  const WaterCell red{waterDepthUnits(2.0f), 230, 20, 20, 255};
  const auto texel = middleOf(renderStill(ctx, {.water = red}));
  CHECK(texel[2] > texel[0] + 40);
  CHECK(texel[2] > texel[1] + 40);
}

// Req: docs/engine/water.md — the simulated ripples reach the screen: a
// pushed field draws differently from a still one.
TEST_CASE("WaterRenderer on the GPU: a ripple changes what is drawn",
          "[gpu][water]") {
  GpuTestContext ctx;
  if (!hasWaterPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in water pipeline");
  }
  const PondDraw draw{};
  WaterField pushed = stillField(draw.water);
  REQUIRE(disturbWaterField(pushed, {0.0f, 0.0f}, 1.2f, 0.1f));
  const auto calm = renderPond(ctx, stillField(draw.water), draw);
  const auto rippled = renderPond(ctx, pushed, draw);
  CHECK(differenceOverMiddle(calm, rippled) > 2000);
}

// Req: docs/engine/water.md §5 — HIGH draws detail LOW does not, and FLAT
// draws a still surface with no waves at all.
TEST_CASE("WaterRenderer on the GPU: fidelity sets how much the water moves",
          "[gpu][water]") {
  GpuTestContext ctx;
  if (!hasWaterPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in water pipeline");
  }
  const auto flat = renderStill(ctx, {.fidelity = WaterFidelity::FLAT});
  const auto low = renderStill(ctx, {.fidelity = WaterFidelity::LOW});
  const auto high = renderStill(ctx, {.fidelity = WaterFidelity::HIGH});
  CHECK(offGrey(middleOf(flat), groundGrey(ctx)) > 20);
  CHECK(differenceOverMiddle(low, high) > 2000);
  CHECK(differenceOverMiddle(flat, low) > 2000);
}

// Req: docs/engine/water.md §4 — the water is lit by the scene's lights,
// as the meshes are.
TEST_CASE("WaterRenderer on the GPU: a red light reddens the water",
          "[gpu][water]") {
  GpuTestContext ctx;
  if (!hasWaterPipeline(ctx.device())) {
    SKIP("no GPU device with a built-in water pipeline");
  }
  const MeshLight lit[] = {SIDE_LIGHTS[0], redLamp()};
  const WaterCell murky = waterOf(1.0f, 255);
  const auto plain = middleOf(renderStill(ctx, {.water = murky}));
  const auto red = middleOf(renderStill(ctx, {.water = murky, .lights = lit}));
  CHECK(red[2] > plain[2] + 20);
  CHECK(std::abs(red[0] - plain[0]) < 12);
}
