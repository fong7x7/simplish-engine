#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-renderer.h>

using namespace eng;

namespace {

/// A renderer in unit-test mode, with a few quads already emitted.
struct SplitFixture {
  GuiRendererContext renderer;

  SplitFixture() {
    REQUIRE(renderer.init(nullptr));
    renderer.viewport_width = 800;
    renderer.viewport_height = 600;
    renderer.beginFrame();
  }
  ~SplitFixture() { renderer.shutdown(); }
  SplitFixture(const SplitFixture&) = delete;
  SplitFixture& operator=(const SplitFixture&) = delete;
  SplitFixture(SplitFixture&&) = delete;
  SplitFixture& operator=(SplitFixture&&) = delete;

  void quad(float x) {
    renderer.emitQuad({{x, 0.0f, 10.0f, 10.0f}, 0xFFFFFFFFU, 0.0f, 0.0f});
  }
};

}  // namespace

TEST_CASE("an unmarked frame draws entirely over the scene") {
  SplitFixture fx;
  fx.quad(0.0f);
  fx.quad(20.0f);
  // Everything after the split draws over; with no split marked, that is
  // the whole frame, which is how a frame with no 3D behaves.
  REQUIRE(fx.renderer.sceneSplit() == fx.renderer.commands.size());
}

TEST_CASE("the split records the paint order position it was marked at") {
  SplitFixture fx;
  // Contiguous quads merge into one batch, so the under and over halves are
  // separated the way the viewport separates them: by a scissor, which ends
  // a batch. Without that the split would fall inside a single command and
  // could not be submitted as two ranges.
  fx.quad(0.0f);
  fx.renderer.pushScissor({0.0f, 0.0f, 100.0f, 100.0f});
  fx.renderer.popScissor();
  const size_t under = fx.renderer.commands.size();
  fx.renderer.markSceneSplit();
  fx.quad(40.0f);

  REQUIRE(fx.renderer.sceneSplit() == under);
  REQUIRE(fx.renderer.sceneSplit() < fx.renderer.commands.size());
}

TEST_CASE("marking before anything is drawn puts the whole frame over") {
  SplitFixture fx;
  fx.renderer.markSceneSplit();
  fx.quad(0.0f);
  REQUIRE(fx.renderer.sceneSplit() == 0);
}

TEST_CASE("beginFrame clears the split with the stream it describes") {
  SplitFixture fx;
  fx.quad(0.0f);
  fx.renderer.markSceneSplit();
  fx.renderer.beginFrame();

  // A stale split would slice the next frame at a position that no longer
  // means anything.
  REQUIRE(fx.renderer.sceneSplit() == 0);
  REQUIRE(fx.renderer.commands.empty());
}
