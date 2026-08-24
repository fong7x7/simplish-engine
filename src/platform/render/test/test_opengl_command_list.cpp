#include <catch2/catch_test_macros.hpp>

// OpenGL command-list tests require the OpenGL backend.
// Guard with ENGINE_RENDERER_OPENGL so the file compiles without it.
#ifdef ENGINE_RENDERER_OPENGL

#include <engine/render/backends/opengl/opengl-command-list.h>
#include <engine/render/backends/opengl/opengl-command.h>

using namespace eng;
using namespace eng::render;

// Req: docs/engine/rendering/pipeline.md §1 — Command recording
// Req: docs/technical-approaches/engine/rendering/opengl-backend.md §3.2 —
//   Deferred command recording

// ============================================================================
// Recording lifecycle
// ============================================================================

TEST_CASE("OpenGlCommandList: begin clears previous commands",
          "[opengl][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §5.2 —
  //   begin() clears command vector
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.bindPipeline(1);
  cmd.end();
  REQUIRE(cmd.commands().size() == 1);

  cmd.begin();
  REQUIRE(cmd.commands().empty());
}

TEST_CASE("OpenGlCommandList: commands returns empty after begin",
          "[opengl][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §5.2
  OpenGlCommandList cmd;
  cmd.begin();
  REQUIRE(cmd.commands().empty());
}

// ============================================================================
// Render pass recording
// ============================================================================

TEST_CASE("OpenGlCommandList: beginRenderPass records command",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Render pass begin
  OpenGlCommandList cmd;
  cmd.begin();
  RhiRenderPassBeginInfo info;
  info.clear_color[0] = 1.0f;
  cmd.beginRenderPass(info);
  cmd.end();

  REQUIRE(cmd.commands().size() == 1);
  REQUIRE(std::holds_alternative<GlCmdBeginRenderPass>(cmd.commands()[0]));

  auto& recorded = std::get<GlCmdBeginRenderPass>(cmd.commands()[0]);
  REQUIRE(recorded.info.clear_color[0] == 1.0f);
}

TEST_CASE("OpenGlCommandList: endRenderPass records command",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Render pass end
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.endRenderPass();
  cmd.end();

  REQUIRE(cmd.commands().size() == 1);
  REQUIRE(std::holds_alternative<GlCmdEndRenderPass>(cmd.commands()[0]));
}

// ============================================================================
// Pipeline binding
// ============================================================================

TEST_CASE("OpenGlCommandList: bindPipeline records handle",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Pipeline binding
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.bindPipeline(42);
  cmd.end();

  REQUIRE(cmd.commands().size() == 1);
  auto& recorded = std::get<GlCmdBindPipeline>(cmd.commands()[0]);
  REQUIRE(recorded.pipeline == 42);
}

// ============================================================================
// Resource binding
// ============================================================================

TEST_CASE("OpenGlCommandList: bindVertexBuffer records handle and offset",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Vertex buffer binding
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.bindVertexBuffer(10, 256);
  cmd.end();

  auto& recorded = std::get<GlCmdBindVertexBuffer>(cmd.commands()[0]);
  REQUIRE(recorded.buffer == 10);
  REQUIRE(recorded.offset == 256);
}

TEST_CASE("OpenGlCommandList: bindIndexBuffer records handle offset and type",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Index buffer binding
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.bindIndexBuffer(20, 128, RhiIndexType::UINT32);
  cmd.end();

  auto& recorded = std::get<GlCmdBindIndexBuffer>(cmd.commands()[0]);
  REQUIRE(recorded.buffer == 20);
  REQUIRE(recorded.offset == 128);
  REQUIRE(recorded.index_type == RhiIndexType::UINT32);
}

TEST_CASE("OpenGlCommandList: bindDescriptorSet records set index and handle",
          "[opengl][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §8 —
  //   No descriptor sets (stub)
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.bindDescriptorSet(0, 99);
  cmd.end();

  auto& recorded = std::get<GlCmdBindDescriptorSet>(cmd.commands()[0]);
  REQUIRE(recorded.set_index == 0);
  REQUIRE(recorded.set == 99);
}

// ============================================================================
// Viewport and scissor
// ============================================================================

TEST_CASE("OpenGlCommandList: setViewport records viewport params",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Viewport configuration
  OpenGlCommandList cmd;
  cmd.begin();
  RhiViewport vp{10.0f, 20.0f, 800.0f, 600.0f, 0.0f, 1.0f};
  cmd.setViewport(vp);
  cmd.end();

  auto& recorded = std::get<GlCmdSetViewport>(cmd.commands()[0]);
  REQUIRE(recorded.viewport.x == 10.0f);
  REQUIRE(recorded.viewport.y == 20.0f);
  REQUIRE(recorded.viewport.width == 800.0f);
  REQUIRE(recorded.viewport.height == 600.0f);
}

TEST_CASE("OpenGlCommandList: setScissor records scissor params",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Scissor configuration
  OpenGlCommandList cmd;
  cmd.begin();
  RhiScissor sc{5, 10, 640, 480};
  cmd.setScissor(sc);
  cmd.end();

  auto& recorded = std::get<GlCmdSetScissor>(cmd.commands()[0]);
  REQUIRE(recorded.scissor.x == 5);
  REQUIRE(recorded.scissor.y == 10);
  REQUIRE(recorded.scissor.width == 640);
  REQUIRE(recorded.scissor.height == 480);
}

// ============================================================================
// Draw commands
// ============================================================================

TEST_CASE("OpenGlCommandList: draw records all params",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Non-indexed draw
  OpenGlCommandList cmd;
  cmd.begin();
  RhiDrawParams params{36, 2, 0, 0};
  cmd.draw(params);
  cmd.end();

  auto& recorded = std::get<GlCmdDraw>(cmd.commands()[0]);
  REQUIRE(recorded.params.vertex_count == 36);
  REQUIRE(recorded.params.instance_count == 2);
}

TEST_CASE("OpenGlCommandList: drawIndexed records all params",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Indexed draw
  OpenGlCommandList cmd;
  cmd.begin();
  RhiDrawIndexedParams params{6, 1, 0, 0, 0};
  cmd.drawIndexed(params);
  cmd.end();

  auto& recorded = std::get<GlCmdDrawIndexed>(cmd.commands()[0]);
  REQUIRE(recorded.params.index_count == 6);
  REQUIRE(recorded.params.instance_count == 1);
}

// ============================================================================
// Compute dispatch
// ============================================================================

TEST_CASE("OpenGlCommandList: dispatch records group counts",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Compute dispatch
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.dispatch(4, 2, 1);
  cmd.end();

  auto& recorded = std::get<GlCmdDispatch>(cmd.commands()[0]);
  REQUIRE(recorded.groups_x == 4);
  REQUIRE(recorded.groups_y == 2);
  REQUIRE(recorded.groups_z == 1);
}

// ============================================================================
// Copy commands
// ============================================================================

TEST_CASE("OpenGlCommandList: copyBuffer records copy params",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Buffer copy
  OpenGlCommandList cmd;
  cmd.begin();
  RhiCopyBufferParams params{1, 2, 512, 0, 64};
  cmd.copyBuffer(params);
  cmd.end();

  auto& recorded = std::get<GlCmdCopyBuffer>(cmd.commands()[0]);
  REQUIRE(recorded.params.src == 1);
  REQUIRE(recorded.params.dst == 2);
  REQUIRE(recorded.params.size == 512);
  REQUIRE(recorded.params.src_offset == 0);
  REQUIRE(recorded.params.dst_offset == 64);
}

TEST_CASE("OpenGlCommandList: copyTextureToBuffer records handles",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering.md §2.1 — Capture readback
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.copyTextureToBuffer(10, 20);
  cmd.end();

  auto& recorded = std::get<GlCmdCopyTextureToBuffer>(cmd.commands()[0]);
  REQUIRE(recorded.src == 10);
  REQUIRE(recorded.dst == 20);
}

// ============================================================================
// Barriers
// ============================================================================

TEST_CASE("OpenGlCommandList: textureBarrier records layout transition",
          "[opengl][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §8 —
  //   No explicit barriers (coarse GL memory barrier)
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.textureBarrier(5, RhiTextureLayout::UNDEFINED,
                     RhiTextureLayout::RENDER_TARGET);
  cmd.end();

  auto& recorded = std::get<GlCmdTextureBarrier>(cmd.commands()[0]);
  REQUIRE(recorded.texture == 5);
  REQUIRE(recorded.old_layout == RhiTextureLayout::UNDEFINED);
  REQUIRE(recorded.new_layout == RhiTextureLayout::RENDER_TARGET);
}

// ============================================================================
// Multi-command recording sequence
// ============================================================================

// Algorithm: Full pass record; REQUIRE command deque size matches draw steps.
TEST_CASE("OpenGlCommandList: full render pass sequence records all commands",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Full frame recording
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.beginRenderPass({});
  cmd.bindPipeline(1);
  cmd.bindVertexBuffer(2, 0);
  cmd.bindIndexBuffer(3, 0, RhiIndexType::UINT16);
  cmd.setViewport({0, 0, 800, 600, 0, 1});
  cmd.setScissor({0, 0, 800, 600});
  cmd.draw({3, 1, 0, 0});
  cmd.drawIndexed({6, 1, 0, 0, 0});
  cmd.endRenderPass();
  cmd.end();

  REQUIRE(cmd.commands().size() == 9);
  REQUIRE(std::holds_alternative<GlCmdBeginRenderPass>(cmd.commands()[0]));
  REQUIRE(std::holds_alternative<GlCmdBindPipeline>(cmd.commands()[1]));
  REQUIRE(std::holds_alternative<GlCmdBindVertexBuffer>(cmd.commands()[2]));
  REQUIRE(std::holds_alternative<GlCmdBindIndexBuffer>(cmd.commands()[3]));
  REQUIRE(std::holds_alternative<GlCmdSetViewport>(cmd.commands()[4]));
  REQUIRE(std::holds_alternative<GlCmdSetScissor>(cmd.commands()[5]));
  REQUIRE(std::holds_alternative<GlCmdDraw>(cmd.commands()[6]));
  REQUIRE(std::holds_alternative<GlCmdDrawIndexed>(cmd.commands()[7]));
  REQUIRE(std::holds_alternative<GlCmdEndRenderPass>(cmd.commands()[8]));
}

TEST_CASE("OpenGlCommandList: commands persist after end until next begin",
          "[opengl][command-list]") {
  // Req: docs/technical-approaches/engine/rendering/opengl-backend.md §5.2
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.bindPipeline(1);
  cmd.draw({3, 1, 0, 0});
  cmd.end();

  REQUIRE(cmd.commands().size() == 2);
}

// ============================================================================
// Edge cases
// ============================================================================

TEST_CASE("OpenGlCommandList: zero-vertex draw is recorded",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Draw with zero vertices
  OpenGlCommandList cmd;
  cmd.begin();
  RhiDrawParams params{0, 0, 0, 0};
  cmd.draw(params);
  cmd.end();

  REQUIRE(cmd.commands().size() == 1);
  auto& recorded = std::get<GlCmdDraw>(cmd.commands()[0]);
  REQUIRE(recorded.params.vertex_count == 0);
}

TEST_CASE("OpenGlCommandList: dispatch with default Y/Z groups",
          "[opengl][command-list]") {
  // Req: docs/engine/rendering/pipeline.md §1 — Compute dispatch defaults
  OpenGlCommandList cmd;
  cmd.begin();
  cmd.dispatch(8, 1, 1);
  cmd.end();

  auto& recorded = std::get<GlCmdDispatch>(cmd.commands()[0]);
  REQUIRE(recorded.groups_x == 8);
  REQUIRE(recorded.groups_y == 1);
  REQUIRE(recorded.groups_z == 1);
}

#endif  // ENGINE_RENDERER_OPENGL
