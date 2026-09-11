#include <cmath>
#include <engine/math/sin-cos.h>
#include <numbers>

namespace eng::math {

namespace {

  /// Radians in one degree.
  constexpr double RADIANS_PER_DEGREE = std::numbers::pi / 180.0;
  /// Degrees in a quarter turn: the unit reduction works in.
  constexpr double QUADRANT_DEGREES = 90.0;
  /// Degrees in a whole turn.
  constexpr double TURN_DEGREES = 360.0;

  /// sin(r) for |r| ≤ π/4: its Taylor series to r¹³, in Horner form. The
  /// first term dropped is below 10⁻¹⁴ there, far under a float's ulp.
  double sinSeries(double r) {
    const double r2 = r * r;
    double p = 1.0 / 6227020800.0;
    p = p * r2 - 1.0 / 39916800.0;
    p = p * r2 + 1.0 / 362880.0;
    p = p * r2 - 1.0 / 5040.0;
    p = p * r2 + 1.0 / 120.0;
    p = p * r2 - 1.0 / 6.0;
    return r + r * r2 * p;
  }

  /// cos(r) for |r| ≤ π/4: its Taylor series to r¹⁴, in Horner form.
  double cosSeries(double r) {
    const double r2 = r * r;
    double p = -1.0 / 87178291200.0;
    p = p * r2 + 1.0 / 479001600.0;
    p = p * r2 - 1.0 / 3628800.0;
    p = p * r2 + 1.0 / 40320.0;
    p = p * r2 - 1.0 / 720.0;
    p = p * r2 + 1.0 / 24.0;
    p = p * r2 - 1.0 / 2.0;
    return 1.0 + r2 * p;
  }

  /// A float with any negative zero made positive, so 90° gives a cosine of
  /// +0 like every other zero — one bit pattern per value in a hash.
  float canonical(double value) {
    return static_cast<float>(value) + 0.0F;
  }

  /// The sine and cosine of an angle `quadrant` quarter turns past the one
  /// whose sine and cosine are @p s and @p c.
  SinCos rotateQuadrants(double s, double c, int quadrant) {
    switch (((quadrant % 4) + 4) % 4) {
      case 1:
        return {canonical(c), canonical(-s)};
      case 2:
        return {canonical(-s), canonical(-c)};
      case 3:
        return {canonical(-c), canonical(s)};
      default:
        return {canonical(s), canonical(c)};
    }
  }

}  // namespace

SinCos sinCosDegrees(float degrees) {
  const double turned = std::fmod(static_cast<double>(degrees), TURN_DEGREES);
  const double quadrant = std::nearbyint(turned / QUADRANT_DEGREES);
  // Exact: both terms are small multiples of a float's precision.
  const double rest = turned - quadrant * QUADRANT_DEGREES;
  const double radians = rest * RADIANS_PER_DEGREE;
  return rotateQuadrants(sinSeries(radians), cosSeries(radians),
                         static_cast<int>(quadrant));
}

}  // namespace eng::math
