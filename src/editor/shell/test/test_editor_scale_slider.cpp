#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-scale-slider.h>

using Catch::Approx;
using namespace eng::editor;

TEST_CASE("a scale of one sits in the exact middle of the slider") {
  REQUIRE(editorScaleSliderFraction(1.0f) == Approx(0.5f));
}

TEST_CASE("the slider's ends are the smallest and largest scale") {
  REQUIRE(editorScaleSliderFraction(EDITOR_SCALE_MIN) == Approx(0.0f));
  REQUIRE(editorScaleSliderFraction(EDITOR_SCALE_MAX) == Approx(1.0f));
  REQUIRE(editorScaleFromSliderFraction(0.0f) == Approx(EDITOR_SCALE_MIN));
  REQUIRE(editorScaleFromSliderFraction(1.0f) == Approx(EDITOR_SCALE_MAX));
}

TEST_CASE("halving and doubling are the same distance either side of one") {
  // The reason the track is logarithmic: scale is felt as a ratio, and on a
  // linear track everything below one would be crammed into its first
  // eighth.
  const float half = editorScaleSliderFraction(0.5f);
  const float twice = editorScaleSliderFraction(2.0f);
  REQUIRE(0.5f - half == Approx(twice - 0.5f));
}

TEST_CASE("a fraction and its scale convert back to each other") {
  for (const float scale : {0.2f, 0.75f, 1.0f, 1.5f, 3.0f, 6.0f}) {
    REQUIRE(editorScaleFromSliderFraction(editorScaleSliderFraction(scale)) ==
            Approx(scale));
  }
}

TEST_CASE("a pointer past either end of the slider holds at that end") {
  // A drag carries on past the track's edge; the value stops at it.
  REQUIRE(editorScaleFromSliderFraction(-0.4f) == Approx(EDITOR_SCALE_MIN));
  REQUIRE(editorScaleFromSliderFraction(1.7f) == Approx(EDITOR_SCALE_MAX));
}

TEST_CASE("a scale outside the range sits at the end it is past") {
  REQUIRE(editorScaleSliderFraction(0.01f) == Approx(0.0f));
  REQUIRE(editorScaleSliderFraction(100.0f) == Approx(1.0f));
}

TEST_CASE("four steps up from one is exactly double") {
  REQUIRE(editorScaleStepped(1.0f, 4) == Approx(2.0f));
  REQUIRE(editorScaleStepped(1.0f, -4) == Approx(0.5f));
}

TEST_CASE("one step is a quarter of a doubling") {
  REQUIRE(editorScaleStepped(1.0f, 1) == Approx(1.18921f).epsilon(1e-4));
}

TEST_CASE("stepping back from a stop lands exactly on the one before") {
  // Stops are fixed points, not a ratio from wherever the value is, so up
  // then down is where it started — to the bit, not to a rounding error.
  REQUIRE(editorScaleStepped(editorScaleStepped(1.0f, 1), -1) == 1.0f);
}

TEST_CASE("a value between stops steps to the nearer stop that way") {
  // A slider can almost never be let go of on one exactly, which is what
  // this is for: 1.03, stepped down, is 1 — not a quarter-doubling below it.
  REQUIRE(editorScaleStepped(1.03f, -1) == 1.0f);
  REQUIRE(editorScaleStepped(0.97f, 1) == 1.0f);
}

TEST_CASE("stepping past either end holds at that end") {
  REQUIRE(editorScaleStepped(EDITOR_SCALE_MAX, 1) == Approx(EDITOR_SCALE_MAX));
  REQUIRE(editorScaleStepped(EDITOR_SCALE_MIN, -1) == Approx(EDITOR_SCALE_MIN));
}

TEST_CASE("no steps leaves the value where it is") {
  REQUIRE(editorScaleStepped(1.37f, 0) == Approx(1.37f));
}
