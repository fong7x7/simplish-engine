#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-renderer.h>

using Catch::Approx;
using namespace eng;

namespace {

/// A renderer in unit-test mode: no device, so nothing is submitted, but the
/// command stream is built exactly as it would be for a real frame.
struct RendererFixture {
  GuiRendererContext renderer;

  RendererFixture() {
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = 800;
    renderer.viewport_height = 600;
    renderer.beginFrame();
  }
  ~RendererFixture() { renderer.shutdown(); }
  RendererFixture(const RendererFixture&) = delete;
  RendererFixture& operator=(const RendererFixture&) = delete;
  RendererFixture(RendererFixture&&) = delete;
  RendererFixture& operator=(RendererFixture&&) = delete;
};

/// Count commands of one type in the stream.
size_t countOfType(const GuiRendererContext& renderer, DrawCommandType type) {
  size_t count = 0;
  for (const auto& cmd : renderer.commands) {
    count += (cmd.type == type) ? 1 : 0;
  }
  return count;
}

constexpr Rect OUTER{100.0f, 100.0f, 400.0f, 300.0f};

}  // namespace

TEST_CASE("pushScissor puts a command in the stream, not just on the stack") {
  RendererFixture fx;
  fx.renderer.pushScissor(OUTER);

  // Without a command nothing reaches RhiCommandList::setScissor, and the
  // clipped content paints over the rest of the frame.
  REQUIRE(fx.renderer.commands.size() == 1);
  REQUIRE(fx.renderer.commands[0].type == DrawCommandType::PUSH_SCISSOR);
  REQUIRE(fx.renderer.commands[0].scissor.x == Approx(OUTER.x));
  REQUIRE(fx.renderer.commands[0].scissor.y == Approx(OUTER.y));
  REQUIRE(fx.renderer.commands[0].scissor.w == Approx(OUTER.w));
  REQUIRE(fx.renderer.commands[0].scissor.h == Approx(OUTER.h));
}

TEST_CASE("popScissor puts a command in the stream") {
  RendererFixture fx;
  fx.renderer.pushScissor(OUTER);
  fx.renderer.popScissor();

  REQUIRE(fx.renderer.commands.size() == 2);
  REQUIRE(fx.renderer.commands[1].type == DrawCommandType::POP_SCISSOR);
}

TEST_CASE("popping the last scissor restores the whole surface") {
  RendererFixture fx;
  fx.renderer.pushScissor(OUTER);
  fx.renderer.popScissor();

  // An empty restore rect is the signal for "back to the whole surface".
  REQUIRE(fx.renderer.commands[1].scissor.w == Approx(0.0f));
  REQUIRE(fx.renderer.commands[1].scissor.h == Approx(0.0f));
}

TEST_CASE("a nested scissor is clipped to its parent") {
  RendererFixture fx;
  fx.renderer.pushScissor(OUTER);
  // Overlaps the parent's right edge by 100px and its top edge by 50px.
  fx.renderer.pushScissor({300.0f, 50.0f, 400.0f, 300.0f});

  const auto& nested = fx.renderer.commands[1].scissor;
  REQUIRE(nested.x == Approx(300.0f));
  REQUIRE(nested.y == Approx(100.0f));
  REQUIRE(nested.w == Approx(200.0f));
  REQUIRE(nested.h == Approx(250.0f));
}

TEST_CASE("popping a nested scissor restores the enclosing rect") {
  RendererFixture fx;
  fx.renderer.pushScissor(OUTER);
  fx.renderer.pushScissor({150.0f, 150.0f, 100.0f, 100.0f});
  fx.renderer.popScissor();

  const auto& restore = fx.renderer.commands.back().scissor;
  REQUIRE(restore.x == Approx(OUTER.x));
  REQUIRE(restore.y == Approx(OUTER.y));
  REQUIRE(restore.w == Approx(OUTER.w));
  REQUIRE(restore.h == Approx(OUTER.h));
}

TEST_CASE("disjoint scissors clip everything out") {
  RendererFixture fx;
  fx.renderer.pushScissor(OUTER);
  fx.renderer.pushScissor({0.0f, 0.0f, 50.0f, 50.0f});

  const auto& nested = fx.renderer.commands[1].scissor;
  REQUIRE(nested.w == Approx(0.0f));
  REQUIRE(nested.h == Approx(0.0f));
}

TEST_CASE("popping an empty stack emits nothing") {
  RendererFixture fx;
  fx.renderer.popScissor();
  REQUIRE(fx.renderer.commands.empty());
}

TEST_CASE("a push past the depth limit emits nothing") {
  RendererFixture fx;
  for (uint32_t i = 0; i < MAX_SCISSOR_DEPTH; ++i) {
    fx.renderer.pushScissor(OUTER);
  }
  const size_t at_limit = fx.renderer.commands.size();
  fx.renderer.pushScissor(OUTER);

  REQUIRE(at_limit == MAX_SCISSOR_DEPTH);
  REQUIRE(fx.renderer.commands.size() == at_limit);
}

TEST_CASE("quads on either side of a scissor stay in separate batches") {
  RendererFixture fx;
  fx.renderer.emitQuad({{0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFFU, 0.0f, 0.0f});
  fx.renderer.pushScissor(OUTER);
  fx.renderer.emitQuad({{0.0f, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFFU, 0.0f, 0.0f});
  fx.renderer.popScissor();

  // Merging the two quads into one batch would put the first inside the
  // scissor, or the second outside it, depending on where the batch landed.
  REQUIRE(countOfType(fx.renderer, DrawCommandType::QUAD_BATCH) == 2);
  REQUIRE(fx.renderer.commands.size() == 4);
}

TEST_CASE("beginFrame clears the scissor stack and the stream") {
  RendererFixture fx;
  fx.renderer.pushScissor(OUTER);
  fx.renderer.beginFrame();

  REQUIRE(fx.renderer.commands.empty());
  REQUIRE(fx.renderer.scissor_stack.depth == 0);
}
