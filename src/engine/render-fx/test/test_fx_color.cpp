#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/render-fx/fx-color.h>

using Catch::Approx;
using namespace eng;

TEST_CASE("mixFxColor is each end at 0 and 1, and linear between",
          "[render-fx][color]") {
  const FxColor from{1.0f, 0.5f, 0.0f, 0.0f};
  const FxColor to{0.0f, 0.5f, 1.0f, 1.0f};

  const FxColor start = mixFxColor(from, to, 0.0f);
  CHECK(start.r == 1.0f);
  CHECK(start.a == 0.0f);
  const FxColor end = mixFxColor(from, to, 1.0f);
  CHECK(end.b == 1.0f);
  CHECK(end.a == 1.0f);
  const FxColor half = mixFxColor(from, to, 0.5f);
  CHECK(half.r == Approx(0.5f));
  CHECK(half.g == Approx(0.5f));
  CHECK(half.b == Approx(0.5f));
  CHECK(half.a == Approx(0.5f));
}
