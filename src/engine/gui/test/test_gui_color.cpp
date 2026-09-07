#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdint>
#include <engine/gui/gui-color.h>

using Catch::Approx;
using namespace eng;

namespace {

/// The sRGB transfer function, which is what an sRGB render target applies
/// when it writes. A clear value has to survive this and come back as the
/// byte it was authored as.
uint8_t encodeToSrgbByte(float linear) {
  const float encoded = linear <= 0.0031308F
                            ? 12.92F * linear
                            : 1.055F * std::pow(linear, 1.0F / 2.4F) - 0.055F;
  return static_cast<uint8_t>(std::lround(encoded * 255.0F));
}

}  // namespace

TEST_CASE("the endpoints of the range are preserved") {
  REQUIRE(srgbByteToLinear(0) == Approx(0.0F));
  REQUIRE(srgbByteToLinear(255) == Approx(1.0F));
}

TEST_CASE("dark values use the linear segment") {
  // Below the cutoff sRGB is a straight line, not a power curve.
  REQUIRE(srgbByteToLinear(10) == Approx((10.0F / 255.0F) / 12.92F));
}

TEST_CASE("mid values use the power segment") {
  // Mid grey sits well under half in linear light; this is the whole reason
  // a clear value cannot be the byte over 255.
  REQUIRE(srgbByteToLinear(128) == Approx(0.2158F).margin(1e-3));
}

TEST_CASE("a cleared colour comes back as the byte it was authored as") {
  // The bug this exists to prevent: an sRGB target encodes on write, so
  // passing the raw byte cleared to a visibly lighter shade. Every byte
  // must survive the round trip.
  for (int value = 0; value <= 255; ++value) {
    const auto byte = static_cast<uint8_t>(value);
    REQUIRE(encodeToSrgbByte(srgbByteToLinear(byte)) == byte);
  }
}

TEST_CASE("the editor's viewport background survives the round trip") {
  // The colour that was wrong on screen: 22 cleared as 83.
  REQUIRE(encodeToSrgbByte(srgbByteToLinear(22)) == 22);
  REQUIRE(encodeToSrgbByte(srgbByteToLinear(26)) == 26);
}

TEST_CASE("the conversion is monotonic") {
  float previous = -1.0F;
  for (int value = 0; value <= 255; ++value) {
    const float linear = srgbByteToLinear(static_cast<uint8_t>(value));
    REQUIRE(linear > previous);
    previous = linear;
  }
}
