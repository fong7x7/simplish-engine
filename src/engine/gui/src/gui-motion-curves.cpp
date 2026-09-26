#include <algorithm>
#include <cmath>
#include <engine/gui/gui-cubic-bezier.h>
#include <engine/gui/gui-spring.h>

namespace eng {

namespace {

  /// Newton steps tried before bisecting.
  constexpr int NEWTON_STEPS = 6;
  /// Bisection steps: enough for a pixel over any GUI duration.
  constexpr int BISECT_STEPS = 24;
  /// How close is close enough, in time.
  constexpr float SOLVE_EPSILON = 1e-5f;
  /// How far from rest a settled spring may be.
  constexpr float SETTLE_BAND = 1e-3f;
  /// How far a critically damped settle search steps, in units of 1 / w0.
  constexpr float SETTLE_STEP = 0.05f;

  /// One coordinate of a curve from 0 to 1 through @p a and @p b, at @p s.
  float bezier(float a, float b, float s) {
    const float inv = 1.0f - s;
    return 3.0f * inv * inv * s * a + 3.0f * inv * s * s * b + s * s * s;
  }

  /// Its slope at @p s.
  float bezierSlope(float a, float b, float s) {
    const float inv = 1.0f - s;
    return 3.0f * inv * inv * a + 6.0f * inv * s * (b - a) +
           3.0f * s * s * (1.0f - b);
  }

  /// The curve parameter where @p curve's x is @p t, by bisection.
  float bisect(const GuiCubicBezier& curve, float t) {
    float lo = 0.0f;
    float hi = 1.0f;
    float s = t;
    for (int i = 0; i < BISECT_STEPS; ++i) {
      s = (lo + hi) * 0.5f;
      (bezier(curve.x1, curve.x2, s) < t ? lo : hi) = s;
    }
    return s;
  }

  /// The curve parameter where @p curve's x is @p t: Newton's method, with
  /// bisection when it stalls on a flat stretch.
  float solveX(const GuiCubicBezier& curve, float t) {
    float s = t;
    for (int i = 0; i < NEWTON_STEPS; ++i) {
      const float err = bezier(curve.x1, curve.x2, s) - t;
      if (std::fabs(err) < SOLVE_EPSILON) {
        return s;
      }
      const float slope = bezierSlope(curve.x1, curve.x2, s);
      if (std::fabs(slope) < SOLVE_EPSILON) {
        break;
      }
      s -= err / slope;
    }
    return bisect(curve, t);
  }

}  // namespace

float GuiCubicBezier::at(float t) const {
  const float ct = std::clamp(t, 0.0f, 1.0f);
  if (ct <= 0.0f || ct >= 1.0f) {
    return ct;
  }
  return bezier(y1, y2, solveX(*this, ct));
}

float GuiSpring::at(float seconds) const {
  const float w0 = std::sqrt(std::max(stiffness, 1e-3f));
  const float zeta = damping / (2.0f * w0);
  const float decay = std::exp(-zeta * w0 * seconds);
  if (zeta >= 1.0f) {
    return 1.0f - decay * (1.0f + w0 * seconds);
  }
  const float wd = w0 * std::sqrt(1.0f - zeta * zeta);
  return 1.0f - decay * (std::cos(wd * seconds) +
                         zeta * w0 / wd * std::sin(wd * seconds));
}

float GuiSpring::settleSeconds() const {
  const float w0 = std::sqrt(std::max(stiffness, 1e-3f));
  const float zeta = damping / (2.0f * w0);
  if (zeta < 1.0f) {
    // The distance from rest is at most the envelope over sqrt(1 - z^2).
    const float amplitude = 1.0f / std::sqrt(1.0f - zeta * zeta);
    return std::log(amplitude / SETTLE_BAND) / (zeta * w0);
  }
  // Critically damped, the distance is e^(-w0 t) (1 + w0 t): step out to it.
  float t = -std::log(SETTLE_BAND) / w0;
  while (1.0f - at(t) > SETTLE_BAND) {
    t += SETTLE_STEP / w0;
  }
  return t;
}

float GuiSpring::eased(float t) const {
  const float ct = std::clamp(t, 0.0f, 1.0f);
  return ct >= 1.0f ? 1.0f : at(ct * settleSeconds());
}

}  // namespace eng
