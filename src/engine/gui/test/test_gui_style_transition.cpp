#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-style-transition.h>

using namespace eng;

namespace {

constexpr GuiStateStyle DARK{.fill = {0, 0, 0}, .radius = 0.0f};
constexpr GuiStateStyle LIGHT{.fill = {200, 200, 200}, .radius = 10.0f};

}  // namespace

TEST_CASE("the first style is taken at once") {
  GuiStyleTransition blend;
  blend.retarget(DARK, 0.1f);
  CHECK_FALSE(blend.isBlending());
  CHECK(blend.current().fill.r == 0);
}

TEST_CASE("a new style is blended to over the duration") {
  GuiStyleTransition blend;
  blend.retarget(DARK, 0.1f);
  blend.retarget(LIGHT, 0.1f);
  CHECK(blend.isBlending());
  blend.tick(0.05f);
  CHECK(blend.current().fill.r > 0);
  CHECK(blend.current().fill.r < 200);
  CHECK(blend.current().radius > 0.0f);
  blend.tick(0.05f);
  CHECK_FALSE(blend.isBlending());
  CHECK(blend.current().fill.r == 200);
  CHECK(blend.current().radius == Catch::Approx(10.0f));
}

TEST_CASE("retargeting mid-blend starts from where it has got to") {
  GuiStyleTransition blend;
  blend.retarget(DARK, 0.1f);
  blend.retarget(LIGHT, 0.1f);
  blend.tick(0.05f);
  const uint8_t midway = blend.current().fill.r;
  blend.retarget(DARK, 0.1f);
  CHECK(blend.current().fill.r == midway);
  blend.tick(0.1f);
  CHECK(blend.current().fill.r == 0);
}

TEST_CASE("the same target again does not restart the blend") {
  GuiStyleTransition blend;
  blend.retarget(DARK, 0.1f);
  blend.retarget(LIGHT, 0.1f);
  blend.tick(0.08f);
  blend.retarget(LIGHT, 0.1f);
  blend.tick(0.02f);
  CHECK_FALSE(blend.isBlending());
}

TEST_CASE("a zero duration snaps") {
  GuiStyleTransition blend;
  blend.retarget(DARK, 0.1f);
  blend.retarget(LIGHT, 0.0f);
  CHECK(blend.current().fill.r == 200);
}
