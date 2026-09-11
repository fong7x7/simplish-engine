#include <bit>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <engine/math/sin-cos.h>

using eng::math::SinCos;
using eng::math::sinCosDegrees;

namespace {

/// How far a result may be from libm's, which is correctly rounded or
/// close to it on every target this runs on: a little over one float ulp
/// at magnitude one.
constexpr double TOLERANCE = 1.5e-7;

/// Whether @p value is within `TOLERANCE` of @p expected.
bool near(float value, double expected) {
  return std::fabs(static_cast<double>(value) - expected) <= TOLERANCE;
}

}  // namespace

TEST_CASE("quarter turns give exact sines and cosines") {
  REQUIRE(sinCosDegrees(0.0F).sin == 0.0F);
  REQUIRE(sinCosDegrees(0.0F).cos == 1.0F);
  REQUIRE(sinCosDegrees(90.0F).sin == 1.0F);
  REQUIRE(sinCosDegrees(90.0F).cos == 0.0F);
  REQUIRE(sinCosDegrees(180.0F).sin == 0.0F);
  REQUIRE(sinCosDegrees(180.0F).cos == -1.0F);
  REQUIRE(sinCosDegrees(-90.0F).sin == -1.0F);
  REQUIRE(sinCosDegrees(450.0F).sin == 1.0F);
}

TEST_CASE("a zero result is always positive zero") {
  REQUIRE_FALSE(std::signbit(sinCosDegrees(90.0F).cos));
  REQUIRE_FALSE(std::signbit(sinCosDegrees(-180.0F).sin));
  REQUIRE_FALSE(std::signbit(sinCosDegrees(270.0F).cos));
}

TEST_CASE("sines and cosines agree with libm to within an ulp") {
  constexpr double RADIANS_PER_DEGREE = 3.14159265358979323846 / 180.0;
  for (int tenth = -7200; tenth <= 7200; tenth += 7) {
    const float degrees = static_cast<float>(tenth) / 10.0F;
    const double radians = static_cast<double>(degrees) * RADIANS_PER_DEGREE;
    const SinCos result = sinCosDegrees(degrees);
    INFO("degrees " << degrees);
    REQUIRE(near(result.sin, std::sin(radians)));
    REQUIRE(near(result.cos, std::cos(radians)));
  }
}

TEST_CASE("sines and cosines keep their golden bits") {
  // Pinned so a change to the series or the reduction is noticed: every
  // replay that turned an actor depends on these exact bits.
  REQUIRE(std::bit_cast<uint32_t>(sinCosDegrees(30.0F).sin) == 0x3F000000U);
  REQUIRE(std::bit_cast<uint32_t>(sinCosDegrees(45.0F).cos) == 0x3F3504F3U);
  REQUIRE(std::bit_cast<uint32_t>(sinCosDegrees(6.0F).sin) == 0x3DD61305U);
  REQUIRE(std::bit_cast<uint32_t>(sinCosDegrees(-123.5F).cos) == 0xBF0D4BBEU);
}
