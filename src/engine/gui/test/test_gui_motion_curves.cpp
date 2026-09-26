#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-cubic-bezier.h>
#include <engine/gui/gui-easing.h>
#include <engine/gui/gui-spring.h>

using namespace eng;
using Catch::Approx;

TEST_CASE("a cubic bezier runs from 0 to 1 along its curve") {
  CHECK(GUI_BEZIER_EASE.at(0.0f) == 0.0f);
  CHECK(GUI_BEZIER_EASE.at(1.0f) == 1.0f);
  // CSS's `ease` is past 80% by half time.
  CHECK(GUI_BEZIER_EASE.at(0.5f) == Approx(0.8024f).margin(1e-3));
  // A straight curve is linear.
  const GuiCubicBezier straight{0.0f, 0.0f, 1.0f, 1.0f};
  CHECK(straight.at(0.3f) == Approx(0.3f).margin(1e-4));
}

TEST_CASE("a back-out curve overshoots and comes back") {
  float peak = 0.0f;
  for (int i = 0; i <= 100; ++i) {
    peak = std::max(peak, GUI_BEZIER_BACK_OUT.at(static_cast<float>(i) / 100));
  }
  CHECK(peak > 1.05f);
  CHECK(GUI_BEZIER_BACK_OUT.at(1.0f) == 1.0f);
}

TEST_CASE("a spring starts still, bounces a little, and settles at rest") {
  const GuiSpring spring{};
  CHECK(spring.at(0.0f) == Approx(0.0f).margin(1e-6));
  float peak = 0.0f;
  for (int i = 0; i <= 100; ++i) {
    peak = std::max(peak, spring.eased(static_cast<float>(i) / 100));
  }
  CHECK(peak > 1.0f);
  CHECK(peak < 1.1f);
  CHECK(spring.at(spring.settleSeconds()) == Approx(1.0f).margin(1e-3));
  CHECK(spring.eased(1.0f) == 1.0f);
}

TEST_CASE("a stiffly damped spring never overshoots") {
  const GuiSpring stiff{.stiffness = 100.0f, .damping = 40.0f};
  for (int i = 0; i <= 100; ++i) {
    CHECK(stiff.eased(static_cast<float>(i) / 100) <= 1.0f);
  }
}

TEST_CASE("every easing starts at 0 and ends at 1") {
  for (const GuiEasing easing :
       {GuiEasing::LINEAR, GuiEasing::EASE_IN, GuiEasing::EASE_OUT,
        GuiEasing::EASE_IN_OUT, GuiEasing::EASE, GuiEasing::EMPHASIZED,
        GuiEasing::BACK_OUT, GuiEasing::SPRING}) {
    CHECK(applyEasing(easing, 0.0f) == Approx(0.0f).margin(1e-5));
    CHECK(applyEasing(easing, 1.0f) == 1.0f);
  }
}
